#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <time.h>

#define NUM_OPERATIONS 150000
#define NUM_THREADS 4

using namespace std;

template<class T>
struct Node {
  T data;
  Node *next;
  Node(T data) {
    this->data = data;
  }
};

// stack object that can hold any type of data
template <class T>
class Stack
{
  private:
    atomic<Node<T>*>* head;
  public:
  // pre allocate nodes and get head initialized
    vector<Node<T>*> preAllocatedNodes;
    Stack() {
      head = new atomic<Node<T>*>;
      for (int i = 0; i < 75000; i++) {
        preAllocatedNodes.push_back(new Node<T>(i));
      }
      for (int i = 0; i < 1000; i++) {
        push(i);
      }
    }

  // numOps counts overall operations and index for the push operation
    atomic<int> numOps {0};
    atomic<int> index {0};

    bool push(T x) {
      Node<T> *current;
      Node<T> *newNode = getNode();
      newNode->data = x;

      do {
        // try to access head to insert
        current = head->load(memory_order_relaxed);
        newNode->next = current;
      } while (!head->compare_exchange_weak(current, newNode, memory_order_release, memory_order_relaxed));

      numOps++;
      return true;
    }
    T pop() {
      Node<T> *current;
      Node<T> *newNode;

      do {
        // try to access head to pop node
        current = head->load(memory_order_relaxed);
        newNode = current->next;
      } while (!head->compare_exchange_weak(current, newNode, memory_order_release,memory_order_relaxed));

      numOps++;
      return current->data;
    }

    int getNumOps() {
      return numOps;
    }

    // get a new node from vector of nodes
    Node<T>* getNode() {
      return preAllocatedNodes.at(index++ % 75000);
    }
};

// random number generator to pick whether to call push or pop
void pushOrPop(Stack<int>* s) {
  srand(time(NULL));
  int pickOp, element;
  for (int i = 0; i < NUM_OPERATIONS; i++) {
    pickOp = rand() % 2;
    if (i % 2 == 0) {
      s->push(i);
    }
    else {
      s->pop();
    }
  }
}

int main() {
  vector<thread> threads;
  Stack<int>* s = new Stack<int>();
  srand(time(NULL));

  // start the timer
  auto startTime = chrono::high_resolution_clock::now();

  for (int i = 0; i < NUM_THREADS; i++)
    threads.push_back(thread(pushOrPop, s));

  auto endTime = chrono::high_resolution_clock::now();
  chrono::duration<double, milli> runtime = endTime - startTime;
  // end the timer and record results
  for (auto& t : threads)
    t.join();

  cout << (runtime.count() * 100);

  return 0;
}
