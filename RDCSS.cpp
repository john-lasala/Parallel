#include <thread>
#include <atomic>
#include <iostream>
#include <chrono>
#include <memory>
#include <vector>

#define NUM_THREADS 16
#define TIME 75.0

#define SET_MARK(_p) (void *)(((uintptr_t)(_p)) | 1)
#define CLR_MARK(_p) (void *)(((uintptr_t)(_p)) & ~1)
#define IS_MARKED(_p) (void *)(((uintptr_t)(_p)) & 1)

using namespace std;

atomic<unsigned long long*> counter {new unsigned long long (0)};
atomic<bool*> flag {new bool(false)};

struct RDCSSDescriptor {

    // set up necessary data for RDCSS
    atomic<void*> *a1;
    atomic<void*> *a2;
    void *o1, *o2, *n2;

    RDCSSDescriptor(atomic<void*>* a1, atomic<void*>* a2, void* o1, void* o2, void* n2):
    a1(a1), a2(a2), o1(o1), o2(o2), n2(n2)
    {}
};

void increment();
void* RDCSSRead(atomic<void*>* a2);
void* RDCSS(atomic<void*>* a1, atomic<void*>* a2, void* o1, void* o2, void* n2);
void* RDCSSShort(RDCSSDescriptor *descriptor);
void* CAS(atomic<RDCSSDescriptor*> *current, RDCSSDescriptor* previous, RDCSSDescriptor* newValue);
void complete(RDCSSDescriptor *descriptor);

void* CAS(atomic<RDCSSDescriptor*> *current, RDCSSDescriptor* previous, RDCSSDescriptor* newValue) {

    current->compare_exchange_weak(previous, newValue, memory_order_release, memory_order_relaxed);

    return previous;
}

void* RDCSSShort(RDCSSDescriptor *descriptor)
{
	void* retVal; 

	do {
		retVal = CAS((atomic<RDCSSDescriptor*>*) &descriptor->a2, (RDCSSDescriptor*)descriptor->o2, (RDCSSDescriptor*)SET_MARK(descriptor));

		if (IS_MARKED(retVal)) {
			complete((RDCSSDescriptor*) retVal);
		}} while (IS_MARKED(retVal));

	if (retVal == (descriptor->o2)) {	
		complete((RDCSSDescriptor*) SET_MARK(descriptor));
	}

	return retVal;
}

void* RDCSS(atomic<void*>* a1, atomic<void*>* a2, void* o1, void* o2, void* n2) {
    
    // set up new descriptor
    RDCSSDescriptor d {a1, a2, o1, o2, n2};
    void* retVal = RDCSSShort(&d);

    return retVal;
}

void complete(RDCSSDescriptor *descriptor) {

    void* val = (descriptor->a1)->load();

    // complete RDCSS
    if (val == descriptor->o1) {
        CAS((atomic<RDCSSDescriptor*>*) descriptor->a2, (RDCSSDescriptor*) SET_MARK(descriptor), (RDCSSDescriptor*) descriptor->n2);
    }
    else {
        CAS((atomic<RDCSSDescriptor*>*) descriptor->o2, (RDCSSDescriptor*) SET_MARK(descriptor), (RDCSSDescriptor*) descriptor->n2);
    }
}

void* RDCSSRead(atomic<void*>* a2) {

    void* retVal;

    do {
        retVal = a2->load(memory_order_relaxed);
        // check if marked, keep spinning while it is
        if (IS_MARKED(retVal)) {
            complete((RDCSSDescriptor*) SET_MARK(retVal));
        }} while(IS_MARKED(retVal));

        return retVal; 
}

void test() {
    // timer for program
    this_thread::sleep_for(chrono::seconds(75));
    *(flag.load()) = true;
}
void increment()
{	
	void* val; 
	void* current;
    bool flagVal = 0;

    // run while flag is false
	while (!*((bool*)(RDCSSRead((atomic<void*>*) &flag))))
	{
		val = RDCSSRead((atomic<void*>*) &counter);

        unsigned long long *newVal = static_cast<unsigned long long*>(val);
        (*newVal)++;

		RDCSS((atomic<void*>*)&flag,(atomic<void*>*) &counter, (void*) flagVal, (void*) val, (void*) *newVal); 
	}
}

int main() {

    vector<thread> threads;

    unsigned long long count = *(counter.load());

	cout << "number of threads: " << NUM_THREADS << endl;

    for (int i = 0; i <= NUM_THREADS; i++) {
        // start the timer
        if (i == NUM_THREADS) {
            threads.push_back(thread(test));
            continue;
        }
        threads.push_back(thread(increment));
    }

    // join all threads
    for (auto& t : threads)
        t.join();
    
    count = *(counter.load());
    cout << "value of counter: " << count << endl;
    return 0;
}
