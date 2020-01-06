#ifndef PAQ8PX_INDIRECTCONTEXT_HPP
#define PAQ8PX_INDIRECTCONTEXT_HPP

template<typename T>
class IndirectContext {
private:
    Array<T> data;
    T *ctx;
    const uint32_t ctxMask, inputMask, inputBits;

public:
    IndirectContext(int bitsPerContext, int inputBits);
    void operator+=(uint32_t i);
    void operator=(uint32_t i);
    T &operator()();
};

template<typename T>
void IndirectContext<T>::operator+=(const uint32_t i) {
  assert(i <= inputMask);
  (*ctx) <<= inputBits;
  (*ctx) |= i;
}

template<typename T>
IndirectContext<T>::IndirectContext(const int bitsPerContext, const int inputBits): data(UINT64_C(1) << bitsPerContext), ctx(&data[0]),
        ctxMask((UINT32_C(1) << bitsPerContext) - 1), inputMask((UINT32_C(1) << inputBits) - 1), inputBits(inputBits) {
  assert(bitsPerContext > 0 && bitsPerContext <= 20);
  assert(inputBits > 0 && inputBits <= 8);
}

template<typename T>
void IndirectContext<T>::operator=(const uint32_t i) {
  ctx = &data[i & ctxMask];
}

template<typename T>
T &IndirectContext<T>::operator()() {
  return *ctx;
}

#endif //PAQ8PX_INDIRECTCONTEXT_HPP
