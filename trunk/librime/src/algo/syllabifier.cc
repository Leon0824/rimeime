// vim: set sts=2 sw=2 et:
// encoding: utf-8
//
// Copyleft 2011 RIME Developers
// License: GPLv3
//
// 2011-07-12 Zou Xu <zouivex@gmail.com>
// 2012-02-11 GONG Chen <chen.sst@gmail.com>
//
#include <functional>
#include <queue>
#include <utility>
#include <vector>
#include <boost/foreach.hpp>
#include <rime/dict/prism.h>
#include <rime/algo/syllabifier.h>

namespace rime {

typedef std::pair<size_t, SpellingType> Vertex;
typedef std::priority_queue<Vertex, std::vector<Vertex>, std::greater<Vertex> > VertexQueue;

int Syllabifier::BuildSyllableGraph(const std::string &input, Prism &prism, SyllableGraph *graph) {
  if (input.empty())
    return 0;

  size_t farthest = 0;
  VertexQueue queue;
  queue.push(Vertex(0, kNormalSpelling));  // start

  while (!queue.empty()) {
    Vertex vertex(queue.top());
    queue.pop();
    size_t current_pos = vertex.first;

    // record a visit to the vertex
    VertexMap::iterator it = graph->vertices.find(current_pos);
    if (it == graph->vertices.end())
      graph->vertices.insert(vertex);  // preferred spelling type comes first
    else
      continue;  // discard worse spelling types

    // see where we can go by advancing a syllable
    std::vector<Prism::Match> matches;
    prism.CommonPrefixSearch(input.substr(current_pos), &matches);
    if (!matches.empty()) {
      EndVertexMap &end_vertices(graph->edges[current_pos]);
      BOOST_FOREACH(const Prism::Match &m, matches) {
        if (m.length == 0) continue;
        size_t end_pos = current_pos + m.length;
        // consume trailing delimiters
        while (end_pos < input.length() &&
               delimiters_.find(input[end_pos]) != std::string::npos)
          ++end_pos;
        if (end_pos > farthest)
          farthest = end_pos;
        SpellingMap &spellings(end_vertices[end_pos]);
        SpellingType end_vertex_type = kInvalidSpelling;
        // when spelling algebra is enabled, a spelling evaluates to a set of syllables;
        // otherwise, it resembles exactly the syllable itself.
        SpellingAccessor accessor(prism.QuerySpelling(m.value));
        while (!accessor.exhausted()) {
          SyllableId syllable_id(accessor.syllable_id());
          SpellingProperties props(accessor.properties());
          props.end_pos = end_pos;
          // add a syllable with properties to the edge's spelling-to-syllable map
          spellings.insert(SpellingMap::value_type(syllable_id, props));
          if (props.type < end_vertex_type) {
            end_vertex_type = props.type;
          }
          accessor.Next();
        }
        // update the vertex type
        if (end_vertex_type < vertex.second) {
          end_vertex_type = vertex.second;
        }
        queue.push(Vertex(end_pos, end_vertex_type));
        EZDBGONLYLOGGERPRINT("added to syllable graph, edge: [%d, %d)", current_pos, end_pos);
      }
    }
  }

  // remove stale vertices and edges
  std::set<int> good;
  good.insert(farthest);
  SpellingType last_type = graph->vertices[farthest];
  for (int i = farthest - 1; i >= 0; --i) {
    if (graph->vertices.find(i) == graph->vertices.end())
      continue;
    // remove stale edges
    for (EndVertexMap::iterator j = graph->edges[i].begin();
         j != graph->edges[i].end(); ) {
      if (good.find(j->first) == good.end()) {
        // not connected
        graph->edges[i].erase(j++);
        continue;
      }
      // remove disqualified syllables (eg. matching abbreviated spellings)
      // when there is a path of more favored type (eg. normal spellings only)
      SpellingType edge_type = kInvalidSpelling;
      for (SpellingMap::iterator k = j->second.begin();
           k != j->second.end(); ) {
        if (k->second.type > last_type) {
          j->second.erase(k++);
        }
        else {
          if (k->second.type < edge_type)
            edge_type = k->second.type;
          ++k;
        }
      }
      if (j->second.empty()) {
        graph->edges[i].erase(j++);
      }
      else {
        if (edge_type == kNormalSpelling)
          CheckOverlappedSpellings(graph, i, j->first);
        ++j;
      }
    }
    if (graph->vertices[i] > last_type || graph->edges[i].empty()) {
      // remove stale vertex
      graph->vertices.erase(i);
      graph->edges.erase(i);
      continue;
    }
    // keep the valid vetex
    good.insert(i);
  }

  if (enable_completion_ && farthest < input.length()) {
    const size_t kExpandSearchLimit = 512;
    std::vector<Prism::Match> keys;
    prism.ExpandSearch(input.substr(farthest), &keys, kExpandSearchLimit);
    if (!keys.empty()) {
      size_t current_pos = farthest;
      size_t end_pos = current_pos;
      size_t code_length = input.length() - farthest;
      EndVertexMap &end_vertices(graph->edges[current_pos]);
      BOOST_FOREACH(const Prism::Match &m, keys) {
        if (m.length < code_length) continue;
        end_pos = input.length();
        SpellingMap &spellings(end_vertices[end_pos]);
        // when spelling algebra is enabled, a spelling evaluates to a set of syllables;
        // otherwise, it resembles exactly the syllable itself.
        SpellingAccessor accessor(prism.QuerySpelling(m.value));
        while (!accessor.exhausted()) {
          SyllableId syllable_id(accessor.syllable_id());
          SpellingProperties props(accessor.properties());
          if (props.type > kNormalSpelling) continue;
          props.type = kCompletion;
          props.credibility *= 0.5;
          props.end_pos = end_pos;
          // add a syllable with properties to the edge's spelling-to-syllable map
          spellings.insert(SpellingMap::value_type(syllable_id, props));
          accessor.Next();
        }
        queue.push(Vertex(end_pos, kCompletion));
        EZDBGONLYLOGGERPRINT("added to syllable graph, compl. edge: [%d, %d)", current_pos, end_pos);
      }
      farthest = end_pos;
    }
  }

  graph->input_length = input.length();
  graph->interpreted_length = farthest;
  EZDBGONLYLOGGERVAR(graph->input_length);
  EZDBGONLYLOGGERVAR(graph->interpreted_length);

  Transpose(graph);

  return farthest;
}

void Syllabifier::CheckOverlappedSpellings(SyllableGraph *graph, size_t start, size_t end) {
  // TODO: more cases to handle...
  if (!graph || graph->edges.find(start) == graph->edges.end())
    return;
  // if "Z" = "YX", mark the vertex between Y and X an ambiguous syllable joint
  EndVertexMap& y_end_vertices(graph->edges[start]);
  // enumerate Ys
  BOOST_FOREACH(const EndVertexMap::value_type& y, y_end_vertices) {
    size_t joint = y.first;
    if (joint >= end) break;
    // test X
    if (graph->edges.find(joint) == graph->edges.end())
      continue;
    EndVertexMap& x_end_vertices(graph->edges[joint]);
    BOOST_FOREACH(const EndVertexMap::value_type& x, x_end_vertices) {
      if (x.first < end) continue;
      if (x.first == end) {
        graph->vertices[joint] = kAmbiguousSpelling;
        EZDBGONLYLOGGERPRINT("ambiguous syllable joint at position %d.", joint);
      }
      break;
    }
  }
}

void Syllabifier::Transpose(SyllableGraph* graph) {
  BOOST_FOREACH(const EdgeMap::value_type& start, graph->edges) {
    SpellingIndex& index(graph->indices[start.first]);
    BOOST_REVERSE_FOREACH(const EndVertexMap::value_type& end, start.second) {
      BOOST_FOREACH(const SpellingMap::value_type& spelling, end.second) {
        SyllableId syll_id = spelling.first;
        index[syll_id].push_back(&spelling.second);
      }
    }
  }
}

}  // namespace rime
