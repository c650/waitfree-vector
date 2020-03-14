template <typename T>
struct vector {

    //data stuff
    T** data;
    size_t sz, cap; //capacity is the actual size of data

    /********* begin constructors *********/
    vector(size_t sz): 
        sz(sz), cap(sz) {
        if(sz < 0) throw; //check for negative size
        data = new T*[cap]();
    }
    vector(void):
        vector(0) {}
    ~vector(void) {
        delete[] data;
    }
    /********* end constructors *********/

    /********* begin internal functions *********/
    void resize(size_t new_cap) {
        if(new_cap < cap) {
            sz = new_cap;
            return;
        }
        T** new_data = new T*[new_cap];
        for(int i = 0; i < cap; ++i) {
            new_data[i] = data[i];
        }
        delete[] data;
        data = new_data;
        cap = new_cap;
    }
    void increase_cap(void) { //this is called when capacity is implicitly increased
        size_t new_cap = cap * 2 + 1;
        resize(new_cap);
    }
    void check_cap(void) {
        if(sz >= cap) {
            increase_cap();
        }
    }
    /********* end internal functions *********/

    /********* begin vector functions *********/
    void push_back(T x) {
        check_cap();
        data[sz++] = new T(x);
    }
    void pop_back(void) {
        if(sz == 0) {
            throw; //empty vector
        }
        --sz;
    }
    void clear(void) {
        resize(0);
    }
    T& at(int pos) {
        if(pos < 0 || pos >= sz || data[pos] == nullptr) {
            throw; //out of bounds
        }
        return *data[pos];
    }
    size_t size(void) {
        return sz;
    }
    size_t capacity(void) {
        return cap;
    }
    T& operator[](int pos) {
        return at(pos);
    }
    /********* end vector functions *********/
};