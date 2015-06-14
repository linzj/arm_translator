#ifndef IRCONTEXT_H
#define IRCONTEXT_H
#ifdef __cplusplus
extern "C" {
#endif
struct IRContext {
    void* m_scanner;
};

enum ContextIRType {
    CONTEXTIR_DEFAULT,
    CONTEXTIR_I64,
    CONTEXTIR_I32,
    CONTEXTIR_I16,
    CONTEXTIR_I8,
    CONTEXTIR_I1,
};

void contextSawIRExit(struct IRContext* context, unsigned long long);
void contextSawRegisterInit(struct IRContext* context, const char* registerName, unsigned long long val);
void contextSawRegisterInitMemory(struct IRContext* context, const char* registerName, unsigned long long size, unsigned long long val);
void contextSawInitOption(struct IRContext* context, const char* opt);

void contextSawCheckRegisterConst(struct IRContext* context, const char* registerName, unsigned long long val);
void contextSawCheckRegister(struct IRContext* context, const char* registerName1, const char* registerName2);
void contextSawCheckState(struct IRContext* context, unsigned long long val1);
void contextSawCheckMemory(struct IRContext* context, const char* name, unsigned long long val2);

void contextSawIRWr(struct IRContext* context, const char* id, void* expr);
void contextSawIRPut(struct IRContext* context, unsigned long long where, void* expr);
void contextSawIRStore(struct IRContext* context, void* valExpr, void* addrExpr);
void contextSawIRLoadG(struct IRContext* context, const char* tmp, void* conditionExpr, void* addrExpr, void* defaultVal);
void contextSawIRStoreG(struct IRContext* context, void* conditionExpr, void* valExpr, void* addrExpr);
void* contextNewConstExpr(struct IRContext* context, unsigned long long val, enum ContextIRType type);
void* contextNewRdTmpExpr(struct IRContext* context, const char* id);
void* contextNewLoadExpr(struct IRContext* context, void* expr, enum ContextIRType type);
void* contextNewGetExpr(struct IRContext* context, const char* regName, enum ContextIRType type);
void contextYYError(int line, int column, struct IRContext* context, const char* reason, const char* text);
#ifdef __cplusplus
}
#endif
#endif /* IRCONTEXT_H */
