#ifndef THREADER_H_
#define THREADER_H_
template<typename C, typename T1, typename T2, typename Fn>
struct ThreadArg {
  C *that;
  Fn fn;
  T1 a1;
  T2 a2;
};
template<typename C, typename Fn>
void *ThreadCall(void *_arg) {
  ThreadArg<C, int, int, Fn> *arg = (ThreadArg<C, int, int, Fn> *) _arg;
  (arg->that->*(arg->fn))();
  delete arg;
  return NULL;
}
template<typename C, typename T1, typename Fn>
void *ThreadCall(void *_arg) {
  ThreadArg<C, T1, int, Fn> *arg = (ThreadArg<C, T1, int, Fn> *) _arg;
  (arg->that->*(arg->fn))(arg->a1);
  delete arg;
  return NULL;
}
template<typename C, typename T1, typename T2, typename Fn>
void *ThreadCall(void *_arg) {
  ThreadArg<C, T1, T2, Fn> *arg = (ThreadArg<C, T1, T2, Fn> *) _arg;
  (arg->that->*(arg->fn))(arg->a1, arg->a2);
  delete arg;
  return NULL;
}
template<typename C>
void NewThread(C *that, void (C::*fn)()) {
  ThreadArg<C, int, int, decltype(fn)> *arg = new ThreadArg<C, int, int, decltype(fn)>();
  arg->that = that;
  arg->fn = fn;
  pthread_t thread;
  pthread_create(&thread, NULL, ThreadCall<C, decltype(fn)>, arg);
}
template<typename C, typename T1>
void NewThread(C *that, void (C::*fn)(T1 a1), const T1 &a1) {
  ThreadArg<C, T1, int, decltype(fn)> *arg = new ThreadArg<C, T1, int, decltype(fn)>();
  arg->that = that;
  arg->fn = fn;
  arg->a1 = a1;
  pthread_t thread;
  pthread_create(&thread, NULL, ThreadCall<C, T1, decltype(fn)>, arg);
}
template<typename C, typename T1, typename T2>
void NewThread(C *that, void (C::*fn)(T1 a1, T2 a2), const T1 &a1, const T2 &a2) {
  ThreadArg<C, T1, T2, decltype(fn)> *arg = new ThreadArg<C, T1, T2, decltype(fn)>();
  arg->that = that;
  arg->fn = fn;
  arg->a1 = a1;
  pthread_t thread;
  pthread_create(&thread, NULL, ThreadCall<C, T1, T2, decltype(fn)>, arg);
}
#endif
