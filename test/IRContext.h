#ifndef IRCONTEXT_H
#define IRCONTEXT_H
#ifdef __cplusplus
extern "C" {
#endif
struct IRContext {
    void* m_scanner;
};

void contextSawRegisterInit(struct IRContext* context, const char* registerName, unsigned long long val);
void contextSawRegisterInitMemory(struct IRContext* context, const char* registerName, unsigned long long size, unsigned long long val);
void contextSawInitOption(struct IRContext* context, const char* opt);

void contextSawCheckRegisterConst(struct IRContext* context, const char* registerName, unsigned long long val);
void contextSawCheckRegister(struct IRContext* context, const char* registerName1, const char* registerName2);
void contextSawCheckState(struct IRContext* context, unsigned long long val1);
void contextSawCheckMemory(struct IRContext* context, const char* name, unsigned long long val2);

void contextYYError(int line, int column, struct IRContext* context, const char* reason, const char* text);
#ifdef __cplusplus
}
#endif
#endif /* IRCONTEXT_H */
