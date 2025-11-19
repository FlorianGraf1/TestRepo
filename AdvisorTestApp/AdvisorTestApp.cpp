// bad_static_analysis_showcase.cpp
// Kompilierbare, absichtlich problematische C++17-Datei für statische Codeanalyse.
// WARNUNG: Nicht als Vorlage verwenden. Enthält UB, Leaks, Datenrennen usw.

#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <thread>
#include <mutex>
#include <fstream>
#include <map>
#include <set>
#include <list>
#include <deque>
#include <chrono>
#include <atomic>
#include <cstdlib>
#include <cassert>
#include <condition_variable>
#include <ctime>

using namespace std;

// --- Gefährliche Makros ---
#define SQUARE(x) x*x
#define UNUSED(x) (void)x
#define MAX(a,b) a>b?a:b
#define BAD_CAST_PTR(t, p) (t*)(p)

// --- Globale Zustände / Datenrennen-Kandidaten ---
int g_counter = 0;
vector<int> g_numbers;
char* g_dangling = nullptr;
atomic<bool> g_run{ true };
mutex g_mtx;
int* g_raw = nullptr;

// --- Forward Dcl ---
struct Base;
void functionWithLeakyException();

// --- Klasse ohne virtuellen Dtor (polymorphe Löschung bleibt “böse”, kompiliert aber) ---
struct Base {
    Base() { cout << "Base()\n"; }
    ~Base() { cout << "~Base()\n"; }
    virtual void foo() { cout << "Base::foo\n"; }
};

struct Derived : Base {
    int* buf;
    Derived() : buf(new int[3]) { cout << "Derived()\n"; }
    ~Derived() {
        // absichtlich falsch: delete statt delete[]
        delete buf;
        cout << "~Derived()\n";
    }
    void foo() override { cout << "Derived::foo\n"; }
};

// --- Schlechtes Rule-of-Five (kompilierbar, aber riskant) ---
struct BadRuleOfFive {
    int* data;
    size_t n;

    BadRuleOfFive(size_t n_) : data(new int[n_]), n(n_) {
        for (size_t i = 0; i < n; i++) data[i] = (int)i;
    }
    // Copy/Move absichtlich weggelassen -> Shallow Copy, Double-Free-Risiko
    ~BadRuleOfFive() {
        delete[] data;
    }
    int& operator[](size_t i) { return data[i]; }
};

// --- Rückgabe Ref auf lokale Var (UB, aber kompiliert) ---
int& refToLocal() {
    int x = 42;
    return x; // UB
}

// --- Rückgabe Zeiger auf Stack (UB) ---
int* ptrToLocal() {
    int y = 7;
    return &y; // UB
}

// --- const_cast-Missbrauch (kompiliert) ---
void breakConstness(string& s) {
    const char* p = s.c_str();
    char* writable = const_cast<char*>(p); // UB, wenn modifiziert
    writable[0] = '\0';
}

// --- Datei-Handle Leck: mit fopen_s unter MSVC, sonst fopen; kein fclose ---
void leakFileHandle(const string& path) {
#if defined(_MSC_VER)
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "w") != 0 || !f) return;
#else
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return;
#endif
    fputs("hello\n", f);
    // absichtlich kein fclose -> Leak
}

// --- Mismatched Allocation ---
void mismatchedAlloc() {
    int* a = new int[10];
    free(a); // falsch
}

// --- OOB ---
void outOfBoundsWrite(vector<int>& v) {
    if (v.empty()) v.resize(2);
    for (size_t i = 0; i <= v.size(); ++i) { // <= -> OOB
        v[i] = (int)i;
    }
}

// --- Iterator-Invalidierung ---
void invalidateIterators() {
    vector<int> v = { 1,2,3 };
    auto it = v.begin();
    v.push_back(4); // evtl. Reallokation
    cout << "Invalidated value: " << *it << "\n"; // UB
}

// --- Strict-Aliasing ---
float nastyStrictAliasing(int x) {
    float* f = (float*)&x; // böse, aber kompiliert
    return *f;
}

// --- Shift über Breite (UB) ---
unsigned badShift(unsigned v) {
    return (unsigned)(v << 40);
}

// --- Null-Pointer-Dereferenz (auskommentiert im main) ---
void nullDeref() {
    int* p = nullptr;
    *p = 123;
}

// --- Double Free / UAF (auskommentiert im main) ---
void doubleFreeUAf() {
    int* p = (int*)malloc(sizeof(int) * 3);
    p[0] = 1; p[1] = 2; p[2] = 3;
    free(p);
    p[1] = 42; // UAF
    free(p);   // Double free
}

// --- Dangling global pointer ---
void makeDangling() {
    char* mem = (char*)malloc(8);
#if defined(_MSC_VER)
    strcpy_s(mem, 8, "hi");
#else
    strcpy(mem, "hi");
#endif
    g_dangling = mem;
    free(mem);
}

// --- printf-Mismatch (kompiliert, aber falsch) ---
void printfMismatch() {
    printf("Number: %s\n", 123); // falsches Format
}

// --- Datenrennen ---
void racingIncrement() {
    for (int i = 0; i < 100000; i++) {
        g_counter++;
    }
}

// --- Schlechtes Locking (auskommentiert im main) ---
void badLockingPattern() {
    g_mtx.lock();
    g_mtx.lock();
    g_numbers.push_back(1);
    g_mtx.unlock();
    g_mtx.unlock();
}

// --- Exceptions + Leak ---
void functionWithLeakyException() {
    int* p = new int[100];
    throw runtime_error("oops"); // Leak
}

int swallowExceptions() {
    try {
        functionWithLeakyException();
    }
    catch (...) {
        return -1;
    }
    return 0;
}

// --- Uninitialisierte Nutzung (kompiliert, aber böse) ---
int uninitUse() {
    int z = 0;
    if (z > 2) {
        return z;
    }
    return 0;
}

// --- map::operator[] Nebenwirkungen ---
void badMapUsage() {
    map<string, int> m;
    m["foo"]++;
    if (m["bar"] == 0) {
        cout << "bar present now\n";
    }
}

// --- Ressourcen ohne RAII ---
void rawFileIO() {
    int sz = 1024;
    char* buf = (char*)malloc(sz);
#if defined(_MSC_VER)
    FILE* f = nullptr;
    if (fopen_s(&f, "out.bin", "wb") == 0 && f) {
        fwrite(buf, 1, sz, f); // buf uninitialisiert
        // kein fclose -> Leak
    }
#else
    FILE* f = fopen("out.bin", "wb");
    if (f) {
        fwrite(buf, 1, sz, f);
        // kein fclose
    }
#endif
    // kein free
}

// --- Überlauf ---
int overflowFun() {
    int a = INT32_MAX;
    int b = 10;
    return a + b;
}

// --- memcpy OOB (auskommentiert im main) ---
void badMemcpy() {
    int a[2] = { 1,2 };
    int b[1];
    memcpy(b, a, sizeof(a));
}

// --- Rückgabewerte ignorieren ---
void ignoreReturnValues() {
    remove("this_file_does_not_exist.txt");
    system("false");
}

// --- Macro-Falle ---
int macroWeird(int x) {
    return SQUARE(x + 1); // -> x+1*x+1
}

// --- reinterpret_cast Unsinn ---
void badReinterpret() {
    double d = 3.14;
    long long* p = reinterpret_cast<long long*>(&d);
    *p = 0xFFFFFFFFFFFFFFFFLL;
}

// --- Volatile-Missbrauch (auskommentiert im main) ---
volatile int g_flag = 0;
void misuseVolatile() {
    while (g_flag == 0) { /* busy wait */ }
}

// --- Pointer serialisieren ---
void writeRawPointer() {
    ofstream out("ptr.txt");
    int* p = new int(123);
    out << p; // Adresse, nicht Wert
    // absichtlich kein close/delete
}

// --- Slicing ---
void takesBaseByValue(Base b) {
    b.foo();
}

// --- Rückgabe moved rref (kompiliert, aber Dangling beim Nutzen) ---
string&& returnMovedRRef() {
    string s = "hello";
    return std::move(s);
}

// --- Pointer in Vector invalidieren ---
void pointerIntoVector() {
    vector<int> v = { 1,2,3 };
    int* p = &v[0];
    v.push_back(4);
    *p = 99;
}

// --- Leerer string write (UB) ---
void emptyStringWrite() {
    string s;
    s.resize(0);
    // erzwinge UB: Zugriff ohne Größe
    // Hinweis: s[0] ist unbegrenzt; kompiliert, kann crashen
    s.reserve(0);
    s[0] = 'x';
}

// --- Signed/Unsigned Fallstrick ---
void signedUnsigned() {
    int n = -1;
    size_t m = 3;
    if ((size_t)n > m) {
        cout << "surprising branch\n";
    }
}

// --- Schlecht initialisierter Static ---
int& badStaticRef() {
    static int* p = new int(5);
    return *p;
}

// --- Thread UAF (auskommentiert im main) ---
void threadUseAfterFree() {
    int* p = new int(7);
    thread t([p] {
        this_thread::sleep_for(chrono::milliseconds(10));
        *p = 99;
        });
    delete p;
    t.join();
}

// --- Doppeltes Unlock (auskommentiert im main) ---
void doubleUnlock() {
    g_mtx.unlock();
}

// --- list erase Fehler (auskommentiert im main) ---
void badListErase() {
    list<int> L = { 1,2,3 };
    auto it = L.begin();
    L.erase(it);
    cout << *it << "\n";
}

// --- Borrowed Buffer zurückgeben ---
char* returnBorrowedBuffer() {
    char tmp[16] = "temp";
    return tmp;
}

// --- 2D-Index OOB (auskommentiert im main) ---
void bad2DIndex() {
    int a[2][2] = { {1,2},{3,4} };
    a[1][2] = 5;
}

// --- Windows/POSIX Zeitkram entfernt, um Portabilitätsfehler zu vermeiden ---

// --- Falscher Delete bei globalem Pointer ---
void badGlobalDelete() {
    g_raw = new int[5];
    delete g_raw; // sollte delete[] sein
    g_raw = nullptr;
}

// --- ifstream offen lassen ---
void readNoClose() {
    ifstream in("missing.txt");
    string s;
    in >> s;
}

// --- „random % 0“ auskommentiert, sonst Compile-Error ---
// int veryBadRandom() { srand((unsigned)time(nullptr)); return rand() % 0; }

// --- Ref nach Scope (kompiliert, böse beim Nutzen) ---
const string& refAfterScope() {
    string a = "local";
    return a;
}

// --- Sequenzpunkt-Falle ---
int sideEffectOrder() {
    int i = 0;
    int arr[3] = { 0,1,2 };
    arr[i] = i++ + arr[i];
    return arr[0];
}

// --- Doppeltes fclose -> bleibt auskommentiert ---
void doubleFclose() {
#if defined(_MSC_VER)
    FILE* f = nullptr;
    if (fopen_s(&f, "abc.txt", "w") != 0 || !f) return;
#else
    FILE* f = fopen("abc.txt", "w");
    if (!f) return;
#endif
    fclose(f);
    fclose(f);
}

// --- advance über Ende ---
void badAdvance() {
    vector<int> v = { 1,2,3 };
    auto it = v.begin();
    advance(it, 10);
    cout << *it << "\n";
}

// --- unique_ptr mit falschem Deleter (kompiliert) ---
struct WrongDeleter {
    void operator()(int* p) const {
        free(p);
    }
};

void badUniquePtr() {
    unique_ptr<int, WrongDeleter> up((int*)malloc(sizeof(int)));
    UNUSED(up);
}

// --- gefährliche Rekursion ---
int dangerousRecursion(int n) {
    int big[10000];
    big[0] = n;
    if (n <= 0) return 0;
    return dangerousRecursion(n - 1) + big[0];
}

// --- string::data schreibend (C++17: data() const) -> const_cast für Kompilierbarkeit ---
void badStringData() {
    string s = "xyz";
    const_cast<char*>(s.data())[1] = 'Q'; // UB, aber kompiliert
}

// --- memmove Overlap (auskommentiert im main) ---
void badMemmove() {
    char buf[8] = "abcdefg";
    memmove(buf, buf + 4, 8);
}

// --- cin ohne Bound (auskommentiert im main) ---
void badCin() {
    char name[8];
    cin >> name;
    cout << "Hello " << name << "\n";
}

// --- Mixed Race ---
int mixedRace = 0;
atomic<int> mixedAtomic{ 0 };
void mixedRacy() {
    for (int i = 0; i < 1000; i++) {
        mixedRace++;
        mixedAtomic++;
    }
}

// --- C-String Vergleich auf Adresse ---
bool badCStringCompare(const char* a, const char* b) {
    return a == b;
}

// --- Adresse von Temporary ---
const char* cstrOfTemporary() {
    return string("temp").c_str();
}

// --- Datei seek ohne Prüfung ---
void badSeek() {
    ifstream in("foo.bin", ios::binary);
    in.seekg(1000000);
    char c{};
    in.read(&c, 1);
}

// --- self move ---
void selfMove() {
    string s = "abc";
    s = std::move(s);
}

// --- condition_variable Missbrauch ---
condition_variable cv;
bool ready = false;

void badCVWait() {
    unique_lock<mutex> lk(g_mtx);
    cv.notify_one();
    cv.wait_for(lk, chrono::milliseconds(1));
    if (!ready) {
        g_numbers.push_back(-1);
    }
}

// --- Spin + global growth (beendet irgendwann) ---
void badSpin() {
    while (g_run) {
        g_numbers.push_back(rand());
        if (g_numbers.size() > 50000) break;
    }
}

// --- Hauptprogramm ---
int main() {
    cout << "Start bad demo\n";

    Base* b = new Derived();
    delete b; // UB-potenzial, aber kompiliert

    BadRuleOfFive a(5);
    {
        BadRuleOfFive c = a; // Shallow Copy
        UNUSED(c[2]);
    }

    int* pl = ptrToLocal(); UNUSED(pl);

    string s = "mutable?";
    breakConstness(s);

    leakFileHandle("leak.txt");
    mismatchedAlloc();

    vector<int> vv = { 0,0 };
    outOfBoundsWrite(vv);

    invalidateIterators();

    cout << "nastyStrictAliasing: " << nastyStrictAliasing(123456) << "\n";
    cout << "badShift: " << badShift(3) << "\n";

    // nullDeref();
    // doubleFreeUAf();

    makeDangling();
    printfMismatch();

    thread t1(racingIncrement), t2(racingIncrement);

    swallowExceptions();
    uninitUse();
    badMapUsage();
    rawFileIO();
    cout << "overflowFun: " << overflowFun() << "\n";
    // badMemcpy();
    ignoreReturnValues();
    cout << "macroWeird(3): " << macroWeird(3) << "\n";
    badReinterpret();
    // misuseVolatile();
    writeRawPointer();
    takesBaseByValue(Derived{});
    // auto&& rr = returnMovedRRef(); cout << rr << "\n";
    pointerIntoVector();
    emptyStringWrite();
    signedUnsigned();
    cout << "badStaticRef: " << badStaticRef() << "\n";
    // threadUseAfterFree();
    // doubleUnlock();
    // badListErase();
    char* bb = returnBorrowedBuffer(); UNUSED(bb);
    // bad2DIndex();
    badGlobalDelete();
    readNoClose();
    // veryBadRandom();
    // const string& badref = refAfterScope(); cout << badref << "\n";
    cout << "sideEffectOrder: " << sideEffectOrder() << "\n";
    // doubleFclose();
    // badAdvance();
    badUniquePtr();
    cout << "dangerousRecursion: " << dangerousRecursion(3) << "\n";
    badStringData();
    // badMemmove();
    // badCin();
    thread t3(mixedRacy), t4(mixedRacy);
    cout << boolalpha << "badCStringCompare(\"a\",\"a\"): " << badCStringCompare("a", "a") << "\n";
    const char* temp = cstrOfTemporary(); UNUSED(temp);
    badSeek();
    selfMove();
    badCVWait();
    badSpin();

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    cout << "g_counter (racy): " << g_counter << "\n";
    cout << "Done bad demo (Verhalten ist absichtlich fragwürdig)\n";
    return 0;
}
