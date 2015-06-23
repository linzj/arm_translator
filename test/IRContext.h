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
void contextSawRegisterInitVec(struct IRContext* context, const char* registerName, void* vec);
void contextSawInitOption(struct IRContext* context, const char* opt);

void contextSawCheckRegisterConst(struct IRContext* context, const char* registerName, unsigned long long val);
void contextSawCheckRegisterFloatConst(struct IRContext* context, const char* registerName, double val);
void contextSawCheckRegister(struct IRContext* context, const char* registerName1, const char* registerName2);
void contextSawCheckState(struct IRContext* context, unsigned long long val1);
void contextSawCheckMemory(struct IRContext* context, const char* name, unsigned long long val2);

void contextYYError(int line, int column, struct IRContext* context, const char* reason, const char* text);

void* contextNumVecAppendInt(struct IRContext* context, void* initList, unsigned long long val);
void* contextNumVecNew(struct IRContext* context, unsigned long long val);
void contextDestoryNumVec(void* initList);
void* contextVecExpr(struct IRContext* context, void* numvec, int type);
#ifdef __cplusplus
}
#endif
#endif /* IRCONTEXT_H */
