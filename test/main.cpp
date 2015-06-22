#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <sys/mman.h>
#include <pthread.h>
#include <memory>
#include <fstream>
#include <streambuf>
#include <string>
#include "IRContextInternal.h"
#include "RegisterInit.h"
#include "RegisterAssign.h"
#include "Check.h"
#include "log.h"
#include "cpuinit.h"
#include "TcgGenerator.h"

extern "C" {
void yyparse(IRContext*);
typedef void* yyscan_t;
int yylex_init(yyscan_t* scanner);

int yylex_init_extra(struct IRContext* user_defined, yyscan_t* scanner);

int yylex_destroy(yyscan_t yyscanner);
void vex_disp_run_translations(uintptr_t* two_words,
    void* guest_state,
    void* host_addr);
void yyset_in(FILE* in_str, yyscan_t yyscanner);
void vex_disp_cp_chain_me_to_slowEP(void);
void vex_disp_cp_chain_me_to_fastEP(void);
void vex_disp_cp_xindir(void);
void vex_disp_cp_xassisted(void);
void vex_disp_cp_evcheck_fail(void);
}

static void initGuestState(CPUARMState& state, const IRContextInternal& context)
{
    RegisterAssign assign;
    for (auto&& ri : context.m_registerInit) {
        ri.m_control->reset();
        uintptr_t val = ri.m_control->init();
        assign.assign(&state, ri.m_name, val);
    }
    // init sp
    state.regs[13] = reinterpret_cast<intptr_t>(malloc(1024));
    state.regs[13] += 1024;
    // init lr
    state.regs[14] = 0xffffffff;
    // set thumb bit
    state.thumb = context.m_thumb;
}

static void checkRun(const char* who, const IRContextInternal& context, const uintptr_t* twoWords, const CPUARMState& guestState)
{
    unsigned checkPassed = 0, checkFailed = 0, count = 0;
    LOGE("checking %s...\n", who);
    for (auto& c : context.m_checks) {
        std::string info;
        if (c->check(&guestState, twoWords, info)) {
            checkPassed++;
        }
        else {
            checkFailed++;
        }
        LOGE("[%u]:%s.\n", ++count, info.c_str());
    }
    LOGE("passed %u, failed %u.\n", checkPassed, checkFailed);
}

static void splitPath(std::string& directory, std::string& fileName, const char* path)
{
    const char* lastSlash = strrchr(path, '/');
    const char* fileNameStart;
    if (lastSlash) {
        directory.assign(path, std::distance(path, lastSlash + 1));
        fileNameStart = lastSlash + 1;
    }
    else {
        fileNameStart = path;
    }
    const char* dot = strrchr(fileNameStart, '.');
    if (dot) {
        fileName.assign(fileNameStart, std::distance(fileNameStart, dot));
    }
    else {
        fileName.assign(fileNameStart);
    }
}

static void assmbleAndLoad(const char* path, std::string& output)
{
    std::string source;
    {
        std::string directory, fileName;
        splitPath(directory, fileName, path);
        source.assign(directory);
        source.append(fileName);
    }
    std::string Ofile(source);
    Ofile.append(".o");
    std::string binFile(source);
    binFile.append(".bin");
    std::string Ssource;
    Ssource.assign(source);
    Ssource.append(".S");

    // do the shell job.
    {
        std::string commandAssemble;
        // arm-linux-androideabi-as test1.S -o test.o
        commandAssemble.append("arm-linux-androideabi-as ");
        commandAssemble.append(Ssource);
        commandAssemble.append(" -mcpu=cortex-a15 -mfloat-abi=hard -mfpu=vfp -meabi=5 --noexecstack -o ");
        commandAssemble.append(Ofile);
        if (system(commandAssemble.c_str())) {
            LOGE("execute command %s fails.\n", commandAssemble.c_str());
            exit(1);
        }
    }
    {
        std::string commandObjCopy;
        // arm-linux-androideabi-objcopy -O binary -j .text test.o test.bin
        commandObjCopy.append("arm-linux-androideabi-objcopy -O binary -j .text ");
        commandObjCopy.append(Ofile);
        commandObjCopy.append(" ");
        commandObjCopy.append(binFile);
        if (system(commandObjCopy.c_str())) {
            LOGE("execute command %s fails.\n", commandObjCopy.c_str());
            exit(1);
        }
    }
    std::ifstream t(binFile, std::ios::in | std::ios::binary);
    std::string str((std::istreambuf_iterator<char>(t)),
        std::istreambuf_iterator<char>());
    output.swap(str);
    {
        // rm the intermediate files
        std::string rmCommand;
        rmCommand.append("rm ");
        rmCommand.append(Ofile);
        rmCommand.append(" ");
        rmCommand.append(binFile);
        if (system(rmCommand.c_str())) {
            LOGE("execute command %s fails.\n", rmCommand.c_str());
            exit(1);
        }
    }
}

static void* worker(void* p)
{
    // assemble and load the binary
    const char* fileName = static_cast<const char*>(p);

    FILE* inputFile = fopen(fileName, "r");
    if (!inputFile) {
        LOGE("fails to open input file: %s.", fileName);
        exit(1);
    }
    std::string binaryCode;
    assmbleAndLoad(fileName, binaryCode);
    IRContextInternal context;
    yylex_init_extra(&context, &context.m_scanner);
    yyset_in(inputFile, context.m_scanner);
    yyparse(&context);
    yylex_destroy(context.m_scanner);

    // allocate executable memory

    // run with llvm
    ARMCPU cpu = { 0 };

    cortex_a15_initfn(&cpu);
    // FIXME: translate here and copy to code to execMem
    initGuestState(cpu.env, context);
    // setup pc
    cpu.env.regs[15] = (uint32_t)(uintptr_t)binaryCode.data();
    uintptr_t twoWords[2];
    void* codeBuffer;
    size_t codeSize;
    jit::TranslateDesc tdesc = { reinterpret_cast<void*>(vex_disp_cp_chain_me_to_fastEP), reinterpret_cast<void*>(vex_disp_cp_xindir), reinterpret_cast<void*>(vex_disp_cp_xassisted) };
    while (cpu.env.regs[15] != 0xfffffffe) {
        struct timespec t2, t1;
        clock_gettime(CLOCK_MONOTONIC, &t1);
        jit::translate(&cpu.env, tdesc, &codeBuffer, &codeSize);
        clock_gettime(CLOCK_MONOTONIC, &t2);
        double t = t2.tv_sec - t1.tv_sec;
        t += static_cast<double>(t2.tv_nsec - t1.tv_nsec) / 1e9;
        LOGE("using %lf seconds to translate.\n", t);
        static const size_t execMemSize = 4096;
        void* execMem = mmap(nullptr, execMemSize, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        EMASSERT(execMem != MAP_FAILED);
        memcpy(execMem, codeBuffer, codeSize);
        free(codeBuffer);
        vex_disp_run_translations(twoWords, &cpu.env, execMem);
        LOGE("%s: status is %d r15 = %08x.\n", fileName, twoWords[0], cpu.env.regs[15]);
        munmap(execMem, execMemSize);
    }
    checkRun("llvm", context, twoWords, cpu.env);
}

int main(int argc, char** argv)
{
    if (argc <= 1) {
        LOGE("need one arg.");
        exit(1);
    }
    std::vector<pthread_t> mythreads;
    for (int i = 1; i < argc; ++i) {
        pthread_t thread;
        if (0 != pthread_create(&thread, nullptr, worker, argv[i])) {
            LOGE("create thread error.\n");
            exit(1);
        }
        mythreads.push_back(thread);
    }
    for (auto t : mythreads) {
        void* threadRet;
        pthread_join(t, &threadRet);
        pthread_detach(t);
    }
    return 0;
}
