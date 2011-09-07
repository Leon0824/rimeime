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
#include <rime/impl/prism.h>
#include <rime/impl/syllablizer.h>

namespace rime {

typedef std::pair<int, SpellingType> Vertex;
typedef std::priority_queue<Vertex, std::vector<Vertex>, std::greater<Vertex> > VertexQueue;

int Syllablizer::BuildSyllableGraph(const std::string &input, Prism &prism, SyllableGraph *graph) {
  if (input.empty())
    return 0;

  int farthest = 0;
  VertexQueue queue;
  queue.push(Vertex(0, kNormalSpelling));  // start

  while (!queue.empty()) {
    Vertex vertex(queue.top());
    queue.pop();
    int current_pos = vertex.first;

    // record a visit to the vertex
    VertexMap::iterator it = graph->vertices.find(current_pos);
    if (it == graph->vertices.end())
      graph->vertices.insert(vertex);
    else if (vertex.second < it->second)  // favor normal spelling
      it->second = vertex.second;

    // see where we can go by advancing a syllable
    std::vector<Prism::Match> matches;
    prism.CommonPrefixSearch(input.substr(current_pos), &matches);
    if (!matches.empty()) {
      EndVertexMap &end_vertices(graph->edges[current_pos]);
      BOOST_FOREACH(const Prism::Match &m, matches) {
        if (m.length == 0) continue;
        int end_pos = current_pos + m.length;
        if (end_pos > farthest)
          farthest = end_pos;
        SpellingMap &spellings(end_vertices[end_pos]);
        // TODO:
        // since SpellingAlgebra is not available yet,
        // we assume spelling resembles exactly the syllable itself.
        SyllableId syllable_id = m.value;
        // add a syllable with default properties to the edge's spelling-to-syllable map
        spellings.insert(SpellingMap::value_type(syllable_id, SpellingProperties()));
        // again, we only have normal spellings for now
        queue.push(Vertex(end_pos, kNormalSpelling));
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
  
  graph->input_length = input.length();
  graph->interpreted_length = farthest;
  
  return farthest;
}

}  // namespace rime
