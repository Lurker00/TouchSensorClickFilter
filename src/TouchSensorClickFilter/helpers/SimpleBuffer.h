#ifndef SimpleBuffer_H_
#define SimpleBuffer_H_

#if defined(min)
#   undef min
#endif

#include <algorithm>

template<typename T>
class SimpleBuffer
{
public:
    explicit SimpleBuffer(size_t size)
    {
        Buffer = new T[size];
        if (Buffer) Size = size;
    }
    ~SimpleBuffer() { delete[] Buffer; }

    const T* Data() const { return Buffer; }
    T* Data() { return Buffer; }

    void Append(const T* data, size_t count)
    {
        size_t to_add = std::min(Size - Pos, count);
        if (to_add)
        {
            memcpy(Buffer + Pos, data, to_add * sizeof(T));
            Pos += to_add;
        }
    }
    void Append(const T& c) { Append(&c, 1); }

private:
    size_t Size = 0;
    size_t Pos  = 0;
    T* Buffer   = nullptr;
};

#endif // SimpleBuffer_H_
