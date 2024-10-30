#ifdef DEBUG
#define DEBUG_LOG(msg) \
    cerr << "[" << __FILE__ << ":" << __LINE__ << "] " << msg << endl
#else
#define DEBUG_LOG(msg)
#endif