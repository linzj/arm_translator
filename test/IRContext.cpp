#include <stdio.h>
#include <string.h>
#include "IRContextInternal.h"
#include "log.h"
#include "Check.h"
#include "RegisterOperation.h"
#include "Vec.h"
#define CONTEXT() \
    static_cast<struct IRContextInternal*>(context)

#define PUSH_BACK_REGINIT(name, val) \
    CONTEXT()                        \
        ->m_registerInit.push_back({ name, val })

#define PUSH_BACK_CHECK(c) \
    CONTEXT()              \
        ->m_checks.push_back(c)

void contextSawRegisterInit(struct IRContext* context, const char* registerName, unsigned long long val)
{
    LOGV("%s: val = %llx.\n", __FUNCTION__, val);
    PUSH_BACK_REGINIT(registerName, RegisterInitControl::createConstantInit(val));
}

void contextSawInitOption(struct IRContext* context, const char* opt)
{
    LOGV("%s: opt = %s.\n", __FUNCTION__, opt);
    if (strcmp(opt, "thumb") == 0) {
        CONTEXT()->m_thumb = true;
        return;
    }
    LOGE("%s:unknow option.\n", __FUNCTION__);
}

void contextSawRegisterInitMemory(struct IRContext* context, const char* registerName, unsigned long long size, unsigned long long val)
{
    LOGV("%s: size = %llx, val = %llx.\n", __FUNCTION__, size, val);
    PUSH_BACK_REGINIT(registerName, RegisterInitControl::createMemoryInit(size, val));
}

void contextSawRegisterInitVec(struct IRContext* context, const char* registerName, void* vec)
{
    LOGV("%s:.\n", __FUNCTION__);
    PUSH_BACK_REGINIT(registerName, RegisterInitControl::createVecInit(vec));
}

void contextSawCheckRegisterConst(struct IRContext* context, const char* registerName, unsigned long long val)
{
    LOGV("%s: registerName = %s, val = %llx.\n", __FUNCTION__, registerName, val);
    PUSH_BACK_CHECK(Check::createCheckRegisterEqConst(registerName, val));
}

void contextSawCheckRegisterFloatConst(struct IRContext* context, const char* registerName, double val)
{
    LOGV("%s: registerName = %s, val = %lf.\n", __FUNCTION__, registerName, val);
    PUSH_BACK_CHECK(Check::createCheckRegisterEqFloatConst(registerName, val));
}

void contextSawCheckVecRegsiterConst(struct IRContext* context, const char* registerName, void* vec)
{
    LOGV("%s: registerName = %s.\n", __FUNCTION__, registerName);
    PUSH_BACK_CHECK(Check::createCheckVecRegisterEqConst(registerName, static_cast<NumberVector*>(vec)));
}

void contextSawCheckRegister(struct IRContext* context, const char* registerName1, const char* registerName2)
{
    LOGV("%s: registerName1 = %s, registerName2 = %s.\n", __FUNCTION__, registerName1, registerName2);
    PUSH_BACK_CHECK(Check::createCheckRegisterEq(registerName1, registerName2));
}

void contextSawCheckState(struct IRContext* context, unsigned long long val)
{
    LOGV("%s: val = %llx.\n", __FUNCTION__, val);
    PUSH_BACK_CHECK(Check::createCheckState(val));
}

void contextSawCheckMemory(struct IRContext* context, const char* registerName, unsigned long long val)
{
    LOGV("%s: registerName = %s, val = %llx.\n", __FUNCTION__, registerName, val);
    PUSH_BACK_CHECK(Check::createCheckMemory(registerName, val));
}

void contextYYError(int line, int column, struct IRContext* context, const char* reason, const char* text)
{
    printf("line %d column %d: error:%s; text: %s.\n", line, column, reason, text);
}

void* contextIntVecAppendInt(struct IRContext* context, void* initList, unsigned long long val)
{
    return appendIntVec(static_cast<IntVec*>(initList), val);
}

void* contextIntVecNew(struct IRContext* context, unsigned long long val)
{
    return createIntVec(val);
}

void* contextVecExpr(struct IRContext* context, void* numvec, int type)
{
    return createNumberVec(static_cast<IntVec*>(numvec), type);
}

void contextDestoryIntVec(void* initList)
{
    delete static_cast<IntVec*>(initList);
}
