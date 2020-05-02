#include <atomic>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <time.h>

#define NUM_OPERATIONS 150000
#define NUM_THREADS 4

using namespace std;

// node holding any type of data
template<class T>
struct Node {
  T data;
  Node *next;
  Node(T data) {
    this->data = data;
  }
};

// write descriptor class for write operations
template<class T>
class WriteDescriptor
{
public:
  // set up the WriteDescriptor with old and new values or nodes
  atomic<Node<T>*> head;
  atomic<bool> flag {true};
  Node<T>* old;
  Node<T>* newNode;
  WriteDescriptor(Node<T>* head, Node<T>* old, Node<T>* newN) {
    this->newNode = newN;
    this->old = old;
    this->head = head;
  }
};

template<class T>
class Descriptor
{
public:
  WriteDescriptor<T>* write;
  atomic<int> size = {0};
  Descriptor(WriteDescriptor<T>* write, int size) {
    this->write = write;
    this->size = size;
  }
};

template <class T>
class Stack
{
private:
  atomic<Node<T>*> head;
  atomic<Descriptor<T>*> stackDescriptor;
public:

  vector<Node<T>*> preAllocatedNodes;
  atomic<int> numOps {0};
  atomic<int> index {0};
  Stack() {
    head = new Node<T>(0);
    stackDescriptor = new Descriptor<T>(NULL, 0);
    for (int i = 0; i < 75000; i++) {
      preAllocatedNodes.push_back(new Node<T>(i));
    }
  }

  bool push(T x) {
    // initialize necessary variables
    WriteDescriptor<T>* writeD;
    Descriptor<T>* currentDescriptor;
    Descriptor<T>* newDescriptor;
    Node<T>* current;
    Node<T>* newNode = getNode();
    newNode->data = x;

    do {
      // complete all writes, and then creator new write with size incrementing
      currentDescriptor = stackDescriptor.load(memory_order_relaxed);
      completeWrite(currentDescriptor->write);
      current = head.load(memory_order_relaxed);
      newNode->next = current;

      writeD = new WriteDescriptor<T>(head.load(memory_order_relaxed),current, newNode);
      newDescriptor = new Descriptor<T>(writeD, currentDescriptor->size+1);

    } while (!stackDescriptor.compare_exchange_weak(currentDescriptor,newDescriptor, memory_order_release, memory_order_relaxed));
    completeWrite(newDescriptor->write);
    numOps++;
    return true;
  }

  T pop() {
    Node<T>* current;
    Node<T>* newNode;
    WriteDescriptor<T>* writeD;
    Descriptor<T>* currentDescriptor;
    Descriptor<T>* newDescriptor;

    do {
      // complete all writes, and then creator new write with size decrementing
      currentDescriptor = stackDescriptor.load(memory_order_relaxed);
      completeWrite(currentDescriptor->write);
      current = head.load(memory_order_relaxed);
      newNode = current->next;

      writeD = new WriteDescriptor<T>(head.load(memory_order_relaxed), current, newNode);
      newDescriptor = new Descriptor<T>(writeD, currentDescriptor->size-1);

    } while (!stackDescriptor.compare_exchange_weak(currentDescriptor,newDescriptor, memory_order_release, memory_order_relaxed));
    completeWrite(newDescriptor->write);
    numOps++;

    return current->data;
  }

  int getNumOps() {
    return numOps;
  }

  int getSize() {
    Descriptor<T>* d = stackDescriptor.load();
    if (d->write == NULL)
      return 0;

    // if flag is false then we have an error, must return size-1 to fix
    if (d->write->flag == false)
      return d->size-1;

    return d->size;
  }

  void completeWrite(WriteDescriptor<T>* write) {
    if (write == NULL)
      return;
    // wait for write to complete
    if (write->flag == false) {
      head.compare_exchange_weak(write->old, write->newNode, memory_order_release,memory_order_relaxed);
      write->flag = true;
    }
  }

  // get random node
  Node<T>* getNode() {
    return preAllocatedNodes.at(index++ % 75000);
  }
};

// function that picks between push,pop, or size
void pushOrPop(Stack<int>* s) {
  srand(time(NULL));
  int pickOp, element;
  for (int i = 0; i < NUM_OPERATIONS; i++) {
    pickOp = rand() % 10;
    // 40% chance for pop and push, 20% chance for size
    if (pickOp >= 0 && pickOp <= 3) {
      s->push(i);
    }
    else if (pickOp >= 4 && pickOp <= 7){
      s->getSize();
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
