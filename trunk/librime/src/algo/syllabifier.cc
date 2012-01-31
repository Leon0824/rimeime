// vim: set sts=2 sw=2 et:
// encoding: utf-8
//
// Copyleft 2011 RIME Developers
// License: GPLv3
//
// 2011-7-12 Zou xu <zouivex@gmail.com>
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
          // add a syllable with properties to the edge's spelling-to-syllable map
          spellings.insert(SpellingMap::value_type(syllable_id, props));
          if (props.type < end_vertex_type) {
            end_vertex_type = props.type;
          }
          accessor.Next();
        }
        // render the vertex type
        if (end_vertex_type < vertex.second) {
          end_vertex_type = vertex.second;
        }
        queue.push(Vertex(end_pos, end_vertex_type));
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
      for (SpellingMap::iterator k = j->second.begin();
           k != j->second.end(); ) {
        if (k->second.type > last_type)
          j->second.erase(k++);
        else
          ++k;
      }
      if (j->second.empty())
        graph->edges[i].erase(j++);
      else
        ++j;
    }
    if (graph->vertices[i] > last_type || graph->edges[i].empty()) {
      // remove stale vertex
      graph->vertices.erase(i);
      graph->edges.erase(i);
      continue;
    }
    // keep the valid vetex
    good.insert(i);
    if (graph->vertices[i] < last_type) {
      last_type = graph->vertices[i];
    }
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
          // add a syllable with properties to the edge's spelling-to-syllable map
          spellings.insert(SpellingMap::value_type(syllable_id, props));
          accessor.Next();
        }
        queue.push(Vertex(end_pos, kCompletion));
      }
      farthest = end_pos;
    }
  }

  graph->input_length = input.length();
  graph->interpreted_length = farthest;

  return farthest;
}

}  // namespace rime
