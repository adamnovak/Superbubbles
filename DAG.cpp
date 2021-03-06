/**
    Superbubbles
    Copyright (C) 2016 Ritu Kundu, Fatima Vayani, Manal Mohamed, Solon P. Pissis 
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/

/** Implements class DAG
 */
#include "DAG.hpp"


namespace supbub{

  DAG::DAG(int64_t n): Graph(n), candidates() {
    //super();
    // Following will be prepared by prepareForSupBub() 
    ordD = nullptr;
    invOrd = nullptr;
    pvsEntrance = nullptr;
    outParent = nullptr;
    outChild = nullptr;
    rmqOutParent = nullptr;
    rmqOutChild = nullptr;
  }

  DAG::~DAG() {
    if (ordD != nullptr) {
      delete[] ordD;
    }
    if (invOrd != nullptr) {
      delete[] invOrd;
    }
    if (pvsEntrance != nullptr) {
      delete[] pvsEntrance;
    }
    if (outParent != nullptr) {
      delete[] outParent;
    }
    if (rmqOutParent != nullptr) {
      delete rmqOutParent;
    }
    if (outChild != nullptr) {
      delete[] outChild;
    }
    if (rmqOutChild != nullptr) {
      delete rmqOutChild;
    }
  }

  int64_t
  DAG::getSourceId(){
    return _numVertices-2; // second last vertex
  }

  int64_t
  DAG::getTerminalId(){
    return _numVertices-1; // last vertex
  }

  void
  DAG::prepareForSupBub(){

    /* Compute topological order */
    invOrd = new int64_t[_numVertices];
    fillTopologicalOrder();

    /* Compute ordD */
    ordD = new int64_t[_numVertices];
    for (int64_t ord = 0; ord < _numVertices; ++ord) {
      ordD[invOrd[ord]] = ord; 
    }

    /* Compute candidate list. Along with it pvsEntrance */
    pvsEntrance = new Candidate*[_numVertices];
    prepareCandListNPvsEntrance();

    /* Compute outParent and outChild and prepare for RMQ */
    outParent = new int64_t[_numVertices];
    outChild = new int64_t[_numVertices];
    prepareOutParNOutChildRMQ();

  }

  Candidate*
  DAG::previousEntrance(int64_t v){
    if (v < _numVertices && v >= 0)  {
      return pvsEntrance[v];
    } else {
      log("Invalid v: ", v);
      return nullptr;
    }
  }

  int64_t
  DAG::vertexAtOrder(int64_t o){
    if (o < _numVertices && o >= 0) {
      return invOrd[o];
    } else {
     log("Invalid order: ", o);
      return -1;
    }
  }

  int64_t
  DAG::rangeMaxOutChild(int64_t start, int64_t end){
    int64_t l_rmq, r_rmq;
    if(start < end){l_rmq=start; r_rmq=end;}
    else{l_rmq=end; r_rmq=start;}
    return outChild[(*rmqOutChild)(r_rmq, l_rmq)];
  }

  int64_t
  DAG::rangeMinOutParent(int64_t start, int64_t end){
    int64_t l_rmq, r_rmq;
    if(start < end){l_rmq=start; r_rmq=end;}
    else{l_rmq=end; r_rmq=start;}
    return outParent[(*rmqOutParent)(l_rmq, r_rmq)];
  }


  //////////////////////// private ////////////////////////

  void
  DAG::fillTopologicalOrder(){
    std::stack<int64_t> ordStack;

    bool* visited = new bool[_numVertices];
    std::fill_n( visited, _numVertices, 0 ); // set to false

    // Call topologicalSort for each unvisited source node
 
    topologicalSort(getSourceId(), visited, ordStack);
  
    // Return result
    int64_t i=0;
    while (!ordStack.empty()) {
      invOrd[i++] = ordStack.top();
      ordStack.pop();
    }

    // clean up;
    delete[] visited;
  }

  void
  DAG::topologicalSort(int64_t v, bool* visited, std::stack<int64_t> &ordStack){
    // Mark the current node as visited.
    visited[v] = true;
 
    // Recursive call for all the children of this vertex
    int64_t_LIST_ITERATOR i;
    if (! _adjList[v].empty()) {
      for (i = _adjList[v].begin(); i != _adjList[v].end(); ++i) {
	if (!visited[*i]){
	  topologicalSort(*i, visited, ordStack);
	}
      }
    }
 
    // Push current vertex to stack which stores result
    ordStack.push(v);

  }


  void
  DAG::prepareCandListNPvsEntrance(){
    int64_t_LIST_ITERATOR i;  
    int64_t ver;
    bool exitDone, entranceDone;
    Candidate* pvsEnt = nullptr;
    for (int64_t ord = 0; ord < _numVertices; ++ord) {  // in topo-order
      ver = invOrd[ord];
      exitDone = false;
      entranceDone= false;    

      // check if an exit candidate
      if (! _parentList[ver].empty()) {
	for (i = _parentList[ver].begin(); !exitDone && i != _parentList[ver].end(); ++i) {
	  if (_outDegree[*i] == 1) {  // a parent with only one child 
	    Candidate* pCand = candidates.insert((int64_t)ver, false,pvsEnt);
	    exitDone = true;
	  }
	}
      }
      
      // check if an entrance candidate
      if (! _adjList[ver].empty()) {
	for (i = _adjList[ver].begin(); !entranceDone && i != _adjList[ver].end(); ++i) {
	  if (_inDegree[*i] == 1) { // a child with only one parent 
	    Candidate* pCand = candidates.insert(ver, true, nullptr);
	    entranceDone = true;
	    // this is previous entrance for coming vertices until next ent
	    pvsEnt = pCand; 
	  }
	}
      }
      // fill previous entrance for this node
      pvsEntrance[ver] = pvsEnt;
    }
  }


  void
  DAG::prepareOutParNOutChildRMQ(){   

    int64_t_LIST_ITERATOR i;
    int64_t minOrd, maxOrd;

    for (int64_t v = 0; v < _numVertices; ++v){
       // fill outParent
      minOrd = _numVertices;
      if (! _parentList[v].empty()) {
	for (i = _parentList[v].begin(); i != _parentList[v].end(); ++i){   
	  if(ordD[(*i)] < minOrd){ // furthest parent
	    minOrd = ordD[(*i)];
	  }
	}
      }
      outParent[ordD[v]] = minOrd;

      // fill outChild
      maxOrd = -1;
      if (! _adjList[v].empty()) {
	for (i = _adjList[v].begin(); i != _adjList[v].end(); ++i){
	  if(ordD[*i] > maxOrd){ // furthest child
	    maxOrd = ordD[*i];
	  }
	}
      }
	outChild[ordD[v]] = maxOrd;
    }

    // prepare for RMQ

    // create a vector of length len and initialize it with 0s
    sdsl::int_vector<> v(_numVertices , 0 ); 
    for(int64_t i = 0; i < _numVertices; i++){
      v[i] = outParent[i];
    }

    rmqOutParent = new sdsl::rmq_succinct_sct<>(&v);

    for(int64_t i = 0; i < _numVertices; i++){
      v[i] = outChild[i];
    }

    rmqOutChild = new sdsl::rmq_succinct_sct<false>(&v);

    sdsl::util::clear(v);
  }

}// end namespace
 
