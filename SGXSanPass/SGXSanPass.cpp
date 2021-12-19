#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "AddressSanitizer.hpp"
#include "SGXSanManifest.h"
#include "AdjustUSP.hpp"

using namespace llvm;

namespace
{
    struct SGXSanPass : public ModulePass
    {
        static char ID;
        SGXSanPass() : ModulePass(ID) {}

        bool runOnModule(Module &M) override
        {
            bool Changed = false;
            AddressSanitizer ASan(M);
            for (Function &F : M)
            {
                StringRef func_name = F.getName();
                // since we have monitored malloc-serial function, (linkonce_odr type function) in library which will check shadowbyte
                // whether instrumented or not is not necessary
                //
                // cauze WhitelistQuery will call sgxsan_printf, so we shouldn't instrument sgxsan_printf with WhitelistQuery
                // (e.g. sgxsan_memcpy_s will call WhitelistQuery)
                if ((not F.isDeclaration()) and (func_name != "ocall_init_shadow_memory") and
                    (func_name != "sgxsan_printf") and (func_name != "sgxsan_ocall_print_string") and
                    (func_name != "sgx_thread_set_multiple_untrusted_events_ocall" /* this may pass sensitive tcs */))
                {
                    // hook sgx-specifical callee, normal asan, elrange check, Out-Addr Whitelist check, GlobalPropageteWhitelist
                    // Sensitive area check, Whitelist fill, Whitelist (De)Active, poison etc.
                    Changed |= ASan.instrumentFunction(F);
                }
                if (func_name == "sgxsan_ocall_print_string")
                {
                    adjustUntrustedSPRegisterAtOcallAllocAndFree(F);
                }
            }
            return Changed;
        }
    }; // end of struct SGXSanPass
} // end of anonymous namespace

char SGXSanPass::ID = 1;
static RegisterPass<SGXSanPass> X("SGXSanPass", "SGXSanPass",
                                  false /* Only looks at CFG */,
                                  false /* Analysis Pass */);

static RegisterStandardPasses Y(
    PassManagerBuilder::EP_EnabledOnOptLevel0, // EP_EarlyAsPossible can only be used in FunctionPass(https://lists.llvm.org/pipermail/llvm-dev/2018-June/123987.html)
    [](const PassManagerBuilder &Builder,
       legacy::PassManagerBase &PM)
    { PM.add(new SGXSanPass()); });