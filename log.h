#ifdef MYDEBUG

#define ASSERT(right_instr) do { \
    if (!right_instr) { \
        fprintf(stderr, "\nAssertion failed: (%s), file <%s>, line: %d\n\n", #right_instr, __FILE__, __LINE__); \
        printf("Current function: %s\n", __PRETTY_FUNCTION__);\
        printf(RED "Link to the line:\n" RESET);          \
        printf("File: " __FILE__ ":%d:1:\n\n", __LINE__); \
        printf(RED "--------------------------------------------------" RESET); \
        abort(); \
    } \
} while (0)
#else
#define ASSERT(right_instr) (void)0
#endif

//-------------------------------------------------------------------------
// LOGGING
//-------------------------------------------------------------------------

#ifdef LOG_MODE

#define STACK_LOGGING(logfile, stk) STACK_LOG(logfile, &(stk), #stk, __FILE__, __PRETTY_FUNCTION__, __LINE__)

#define WRITE_ERROR_LOG(logfile, message) WriteErrorLog(logfile, message, __FILE__, __PRETTY_FUNCTION__)

#else
#define STACK_LOGGING(logfile, stk) (void)0

#define WRITE_ERROR_LOG (void)0

#endif