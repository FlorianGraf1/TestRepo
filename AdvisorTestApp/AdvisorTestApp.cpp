// bad_static_analysis_showcase.cpp
// Zweck: Gezielt fehlerhafter C++-Code, um statische Codeanalyse-Tools zu evaluieren.
// HINWEIS: NICHT als Vorlage verwenden. Absichtlich unsicher, ineffizient und teils undefiniertes Verhalten.
//
// Kompilieren (Beispiel):
//   g++ -std=c++17 -O0 -Wall -Wextra -pthread bad_static_analysis_showcase.cpp -o bad_demo
//
// Ausführen kann abstürzen, hängen, Daten beschädigen oder UB triggern – das ist beabsichtigt.

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

using namespace std;

// --- Gefährliche Makros ---
#define SQUARE(x) x*x            // fehlende Klammern => Präzedenz-Probleme
#define UNUSED(x) (void)x
#define MAX(a,b) a>b?a:b         // auch ohne Klammern
#define BAD_CAST_PTR(t, p) (t*)(p) // C-Style-Cast

// --- Globale Zustände / Datenrennen-Kandidaten ---
int g_counter = 0;                    // wird ohne Synchronisierung inkrementiert
vector<int> g_numbers;                // global modifiziert
char* g_dangling = nullptr;           // verweist später ggf. auf freigegebenen Speicher
atomic<bool> g_run{true};             // halbgar genutzte Atomics
mutex g_mtx;                          // ineffizient/falsch genutzt
int* g_raw = nullptr;                 // roher Speicher, falsches new/delete möglich

// --- Schlechte Forward-Deklaration / implizite Annahmen ---
struct Base;
void functionWithLeakyException();

// --- Klasse ohne virtuellem Destruktor: Polymorphe Löschung fehlschlägt ---
struct Base {
    Base() { cout << "Base()\n"; }
    // KEIN virtual ~Base()
    ~Base() { cout << "~Base()\n"; }
    virtual void foo() { cout << "Base::foo\n"; }
};

struct Derived : Base {
    int* buf;
    Derived() : buf(new int[3]) { cout << "Derived()\n"; }
    ~Derived() { 
        // ABSICHTLICH: delete statt delete[] (Mismatch)
        delete buf; 
        cout << "~Derived()\n"; 
    }
    void foo() override { cout << "Derived::foo\n"; }
};

// --- Schlechte Regel-der-Fünf-Implementierung ---
struct BadRuleOfFive {
    int* data;
    size_t n;

    BadRuleOfFive(size_t n_) : data(new int[n_]), n(n_) {
        for (size_t i=0;i<n;i++) data[i]= (int)i;
    }
    // Fehlender Copy-Konstruktor: implizites Shallow-Copy -> Double Free
    // Fehlender Copy-Assignment: dito
    // Fehlender Move-Konstruktor/-Zuweisung: ineffizient/gefährlich
    ~BadRuleOfFive() {
        // Mögliche Double-Frees, wenn kopiert wurde
        delete[] data;
    }

    int& operator[](size_t i) { return data[i]; } // keine Bounds-Prüfung
};

// --- Rückgabe Referenz auf lokale Variable (dangling reference) ---
int& refToLocal() {
    int x = 42;        // lokale Variable
    return x;          // DANGING REF (UB)
}

// --- Rückgabe Zeiger auf Stack ---
int* ptrToLocal() {
    int y = 7;
    return &y;         // DANGING PTR (UB)
}

// --- Missbrauch von const_cast auf string::c_str() ---
void breakConstness(string& s) {
    const char* p = s.c_str();
    char* writable = const_cast<char*>(p); // UB, wenn Speicher schreibgeschützt
    writable[0] = '\0';                    // zerstört String-Invariante
}

// --- Falsch verwaltete Datei-Handles ---
void leakFileHandle(const string& path) {
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return;
    // nie fclose -> Leak
    fputs("hello\n", f);
}

// --- Mismatched Allocation ---
void mismatchedAlloc() {
    int* a = new int[10];
    free(a); // MISMATCH: new[] vs free
}

// --- Off-by-one / OOB ---
void outOfBoundsWrite(vector<int>& v) {
    if (v.empty()) v.resize(2);
    for (size_t i=0; i<=v.size(); ++i) { // <= statt <
        v[i] = (int)i; // OOB beim letzten Durchlauf
    }
}

// --- Iterator-Invalidierung ---
void invalidateIterators() {
    vector<int> v = {1,2,3};
    auto it = v.begin();
    v.push_back(4); // mögliche Reallokation -> it ungültig
    cout << "Invalidated value: " << *it << "\n"; // UB
}

// --- Unsichere Casts / Strict-Aliasing-Verstöße ---
float nastyStrictAliasing(int x) {
    // punne int als float über C-Style-Cast -> Strict-Aliasing verletzbar
    float* f = (float*)&x;
    return *f; // UB
}

// --- Bit-Schieben über Breite hinaus ---
unsigned badShift(unsigned v) {
    return v << 40; // undefined behavior je nach Typbreite
}

// --- Null-Pointer-Dereferenz ---
void nullDeref() {
    int* p = nullptr;
    *p = 123; // Crash
}

// --- Double Free / Use-after-free ---
void doubleFreeUAf() {
    int* p = (int*)malloc(sizeof(int)*3);
    p[0]=1; p[1]=2; p[2]=3;
    free(p);
    // use-after-free
    p[1] = 42; // UB
    free(p);   // Double free
}

// --- Dangling global pointer ---
void makeDangling() {
    char* mem = (char*)malloc(8);
    strcpy(mem, "hi"); // möglicherweise ohne Platz für \0, hier ok aber unsauber
    g_dangling = mem;
    free(mem); // g_dangling zeigt nun auf freed Speicher
}

// --- Unsichere Formatierung / printf-Mismatch ---
void printfMismatch() {
    printf("Number: %s\n", 123); // falscher Format-String-Typ
}

// --- Datenrennen: mehrere Threads auf globale Variable ---
void racingIncrement() {
    for (int i=0;i<100000;i++) {
        g_counter++; // kein Lock -> Datenrennen
    }
}

// --- Deadlock/Locking-Fehler ---
void badLockingPattern() {
    // zweifaches Lock ohne Notwendigkeit, potenziell Deadlock in anderem Codepfad
    g_mtx.lock();
    g_mtx.lock();
    g_numbers.push_back(1);
    g_mtx.unlock();
    g_mtx.unlock();
}

// --- Schlechte Ausnahmebehandlung ---
void functionWithLeakyException() {
    int* p = new int[100];
    throw runtime_error("oops"); // Leak: p wird nie delete[]
}

int swallowExceptions() {
    try {
        functionWithLeakyException();
    } catch(...) {
        // alle Infos verloren, keine Bereinigung der Ressourcen
        return -1;
    }
    return 0;
}

// --- Uninitialisierte Nutzung ---
int uninitUse() {
    int z;           // uninitialisiert
    if (z > 2) {     // UB: z nicht initialisiert
        return z;
    }
    return 0;
}

// --- Falsche Nutzung von std::map::operator[] (unbeabsichtigte Einfügungen) ---
void badMapUsage() {
    map<string,int> m;
    m["foo"]++;  // erzeugt Eintrag mit 0 und inkrementiert -> ok aber oft unerwünscht
    if (m["bar"] == 0) { // fügt "bar" ein -> Seiteneffekt
        cout << "bar present now\n";
    }
}

// --- Ressourcen ohne RAII ---
void rawFileIO() {
    int sz = 1024;
    char* buf = (char*)malloc(sz);
    FILE* f = fopen("out.bin", "wb");
    if (f) {
        fwrite(buf, 1, sz, f); // buf uninitialisiert
        // fclose vergessen
    }
    // free vergessen
}

// --- Überlauf / absurde Arithmetik ---
int overflowFun() {
    int a = INT32_MAX;
    int b = 10;
    return a + b; // Überlauf
}

// --- Unsichere memcpy ---
void badMemcpy() {
    int a[2] = {1,2};
    int b[1];
    memcpy(b, a, sizeof(a)); // OOB write
}

// --- Schlechte Rückgabewerte ignorieren ---
void ignoreReturnValues() {
    remove("this_file_does_not_exist.txt"); // Rückgabewert ignoriert
    system("false");                         // Rückgabewert ignoriert + system()
}

// --- Schlechte Macro-Nutzung ---
int macroWeird(int x) {
    return SQUARE(x+1); // expands to x+1*x+1 -> falsch
}

// --- Reinterpret-Cast-Unsinn ---
void badReinterpret() {
    double d = 3.14;
    long long* p = reinterpret_cast<long long*>(&d);
    *p = 0xFFFFFFFFFFFFFFFFLL; // zerstört Bitmuster von d
}

// --- Schlechte Verwendung von volatile (falsche Synchronisationsannahmen) ---
volatile int g_flag = 0;
void misuseVolatile() {
    while (g_flag == 0) { /* busy-wait ohne Sleep, ohne Atomics */ }
}

// --- Schlechte Serialisierung von Zeigern ---
void writeRawPointer() {
    ofstream out("ptr.txt");
    int* p = new int(123);
    out << p;   // schreibt Adresse, nicht Wert, nutzlos/gefährlich
    // close vergessen, delete vergessen
}

// --- Slicing durch Wert-Parameter ---
void takesBaseByValue(Base b) { // Slicing, wenn Derived übergeben
    b.foo();
}

// --- Undefiniertes Verhalten durch Lebenszeit nach move ---
string&& returnMovedRRef() {
    string s = "hello";
    return std::move(s); // Rückgabe REF auf temporär -> Dangling
}

// --- Container mit Zeigern auf Elemente, die invalidiert werden ---
void pointerIntoVector() {
    vector<int> v = {1,2,3};
    int* p = &v[0];
    v.push_back(4); // mögliche Reallokation invalidiert p
    *p = 99;        // UB
}

// --- Ungültiger std::string Zugriff via operator[] im leeren String ---
void emptyStringWrite() {
    string s;
    s[0] = 'x'; // UB
}

// --- Signed/Unsigned Vergleichsfallen ---
void signedUnsigned() {
    int n = -1;
    size_t m = 3;
    if ((size_t)n > m) { // n wird zu sehr großem size_t
        cout << "surprising branch\n";
    }
}

// --- Schlecht initialisierter Static ---
int& badStaticRef() {
    static int* p = new int(5);
    return *p; // Leak fürs Leben des Prozesses + pointer static
}

// --- Thread mit Verwendung freigegebener Daten ---
void threadUseAfterFree() {
    int* p = new int(7);
    thread t([p]{
        this_thread::sleep_for(chrono::milliseconds(10));
        *p = 99; // hängt davon ab, ob schon freigegeben
    });
    delete p;   // UAF Rennen
    t.join();
}

// --- Doppeltes Unlock etc. ---
void doubleUnlock() {
    g_mtx.unlock(); // unlock ohne lock -> UB/Abort
}

// --- Schlechte Randfälle bei Deque/List ---
void badListErase() {
    list<int> L = {1,2,3};
    auto it = L.begin();
    L.erase(it);
    // it ist nun ungültig
    cout << *it << "\n"; // UB
}

// --- getcwd/argv/argc Missbrauch (Platzhalter für weitere Fallen) ---
char* returnBorrowedBuffer() {
    char tmp[16] = "temp";
    return tmp; // Dangling
}

// --- Aus Matrix hinaus indizieren ---
void bad2DIndex() {
    int a[2][2] = {{1,2},{3,4}};
    a[1][2] = 5; // OOB
}

// --- Ungültige Zeitfunktionen ---
void unsafeTimeWait() {
    timespec ts;
    ts.tv_sec = -1;          // unsinnig
    ts.tv_nsec = 2000000000; // > 1e9
    // nanosleep(&ts, nullptr); // würde scheitern
}

// --- Falscher Delete bei globalem Pointer ---
void badGlobalDelete() {
    g_raw = new int[5];
    delete g_raw; // sollte delete[] sein
    g_raw = nullptr;
}

// --- Nicht geschlossene ifstream ---
void readNoClose() {
    ifstream in("missing.txt");
    string s;
    in >> s; // ignoriert Fehler, kein close
}

// --- Schlechte Random-/Seed-Nutzung ---
int veryBadRandom() {
    srand(time(nullptr));
    return rand() % 0; // Division durch 0 -> UB
}

// --- Use-after-scope mit Referenz ---
const string& refAfterScope() {
    string a = "local";
    return a; // Dangling Ref
}

// --- Reihenfolgeabhängigkeiten / Nebenwirkungen ---
int sideEffectOrder() {
    int i = 0;
    int arr[3] = {0,1,2};
    arr[i] = i++ + arr[i]; // Sequenzpunkt-Fallen (wohl definiert in C++17, aber fragwürdig)
    return arr[0];
}

// --- Doppelter fclose ---
void doubleFclose() {
    FILE* f = fopen("abc.txt", "w");
    if (!f) return;
    fclose(f);
    fclose(f); // UB
}

// --- Schlechte Nutzung von std::advance / Iteratorgrenzen ---
void badAdvance() {
    vector<int> v = {1,2,3};
    auto it = v.begin();
    advance(it, 10); // über das Ende hinaus
    cout << *it << "\n"; // UB
}

// --- Missbrauch von unique_ptr mit custom Deleter (falsch) ---
struct WrongDeleter {
    void operator()(int* p) const {
        free(p); // sollte delete verwenden
    }
};

void badUniquePtr() {
    unique_ptr<int, WrongDeleter> up((int*)malloc(sizeof(int))); // Mismatch später möglich
    // vergisst Init, kein Nutzen
}

// --- Absichtlich ineffiziente & gefährliche Rekursion ---
int dangerousRecursion(int n) {
    int big[10000]; // großer Stack-Frame
    big[0] = n;
    if (n <= 0) return 0;
    return dangerousRecursion(n-1) + big[0];
}

// --- Schlechte Nutzung von std::string::data() schreibend ---
void badStringData() {
    string s = "xyz";
    char* p = s.data(); // in C++17 ist data() nicht schreibbar garantiert
    p[1] = 'Q';         // potenziell UB
}

// --- Schlechter memmove-Overlap ---
void badMemmove() {
    char buf[8] = "abcdefg";
    memmove(buf, buf+4, 8); // Overlap-Länge falsch -> OOB
}

// --- Fehlerhafte Bounds beim Lesen ---
void badCin() {
    char name[8];
    cin >> name; // keine Längenbegrenzung
    cout << "Hello " << name << "\n";
}

// --- Race mit Atomics + Non-Atomics gemischt ---
int mixedRace = 0;
atomic<int> mixedAtomic{0};
void mixedRacy() {
    for (int i=0;i<1000;i++) {
        mixedRace++;        // non-atomic
        mixedAtomic++;      // atomic
    }
}

// --- Schlechter Vergleich von C-Strings ---
bool badCStringCompare(const char* a, const char* b) {
    return a == b; // vergleicht Adressen, nicht Inhalte
}

// --- Nutzung von address of temporaries ---
const char* cstrOfTemporary() {
    return string("temp").c_str(); // Dangling
}

// --- Datei lesen ohne Prüfung, dann auf Position springen ---
void badSeek() {
    ifstream in("foo.bin", ios::binary);
    in.seekg(1000000); // ignoriert fail()
    char c;
    in.read(&c, 1);    // liest unvalidiert
}

// --- Unnötige self-move ---
void selfMove() {
    string s = "abc";
    s = std::move(s); // Self-move, selten sinnvoll
}

// --- Schlechte Nutzung von condition_variable ohne Mutex ---
#include <condition_variable>
condition_variable cv;
bool ready = false;

void badCVWait() {
    unique_lock<mutex> lk(g_mtx);
    // notify ohne wait und ohne richtige Bedingung
    cv.notify_one();
    // warten auf ready ohne while-Schleife
    cv.wait_for(lk, chrono::milliseconds(1));
    if (!ready) {
        // tue irgendwas falsches
        g_numbers.push_back(-1);
    }
}

// --- Unendliche Schleife mit Modifikation globaler Daten ---
void badSpin() {
    while (g_run) {
        g_numbers.push_back(rand()); // unendlich wachsender Speicher
        if (g_numbers.size() > 100000) break; // halbgarer Abbruch
    }
}

// --- Hauptprogramm, das diverse Fallen triggert ---
int main() {
    cout << "Start bad demo\n";

    // Polymorphe Löschung ohne virtuellen Destruktor
    Base* b = new Derived();
    delete b; // UB: ~Derived möglicherweise nicht gerufen

    // Rule-of-Five Fehler (Shallow Copy -> Double Free)
    BadRuleOfFive a(5);
    {
        BadRuleOfFive c = a;          // implizit kopiert -> Double free später möglich
        UNUSED(c[2]);
    }

    // Dangling reference / pointer
    // WARTE: direkten Aufruf vermeiden, um Crash zu ermöglichen:
    // int& r
