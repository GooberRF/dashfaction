#include "MiniDumpHelper.h"

#define CRASHHANDLER_ERR(msg) MessageBox(nullptr, TEXT(msg), 0, MB_ICONERROR | MB_OK | MB_SETFOREGROUND | MB_TASKMODAL)

//
// This function determines whether we need data sections of the given module
//
bool MiniDumpHelper::is_data_section_needed(const WCHAR* module_name)
{
    // Extract the module name
    WCHAR file_name[_MAX_FNAME] = L"";
    _wsplitpath(module_name, nullptr, nullptr, file_name, nullptr);

    // Compare the name with the list of known names and decide
    for (const auto& name : m_known_modules) {
        if (wcsicmp(file_name, name.c_str()) == 0) {
            return true;
        }
    }

    // Complete
    return false;
}

BOOL CALLBACK MiniDumpHelper::mini_dump_callback(PVOID param, const PMINIDUMP_CALLBACK_INPUT input,
                                                 PMINIDUMP_CALLBACK_OUTPUT output)
{
    MiniDumpHelper* that = reinterpret_cast<MiniDumpHelper*>(param);

    BOOL ret = FALSE;

    // Check parameters
    if (input == 0 || output == 0)
        return FALSE;

    // Process the callbacks
    switch (input->CallbackType) {
    case IncludeModuleCallback: {
        // Include the module into the dump
        ret = TRUE;
    } break;

    case IncludeThreadCallback: {
        // Include the thread into the dump
        ret = TRUE;
    } break;

    case ModuleCallback: {
        // Are data sections available for this module ?
        if (output->ModuleWriteFlags & ModuleWriteDataSeg) {
            // Yes, they are, but do we need them?
            if (!that->is_data_section_needed(input->Module.FullPath)) {
                // wprintf(L"Excluding module data sections: %s \n", Input->Module.FullPath);
                output->ModuleWriteFlags &= (~ModuleWriteDataSeg);
            }
        }

        ret = TRUE;
    } break;

    case ThreadCallback: {
        // Include all thread information into the minidump
        ret = TRUE;
    } break;

    case ThreadExCallback: {
        // Include this information
        ret = TRUE;
    } break;

    case MemoryCallback: {
        // We do not include any information here -> return FALSE
        ret = FALSE;
    } break;

    case CancelCallback:
        break;
    }

    return ret;
}

bool MiniDumpHelper::write_dump(const char* path, PEXCEPTION_POINTERS exception_pointers, HANDLE process,
                               DWORD thread_id)
{
    if (!m_MiniDumpWriteDump)
        return false;

    HANDLE file = nullptr;
    file = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                        nullptr);

    if (INVALID_HANDLE_VALUE == file) {
        char buf[256];
        sprintf(buf, "Error %lu! CreateFile failed when writing a Minidump.", GetLastError());
        CRASHHANDLER_ERR(buf);
        return false;
    }

    MINIDUMP_EXCEPTION_INFORMATION exc_info;
    exc_info.ThreadId = thread_id;
    exc_info.ExceptionPointers = exception_pointers;
    exc_info.ClientPointers = TRUE;

    MINIDUMP_TYPE dump_type = MiniDumpNormal;
    MINIDUMP_CALLBACK_INFORMATION callback_info;
    MINIDUMP_CALLBACK_INFORMATION* callback_info_ptr = nullptr;

    // See http://www.debuginfo.com/articles/effminidumps2.html
    if (m_info_level == 0) {
        dump_type = MiniDumpWithIndirectlyReferencedMemory;
        callback_info_ptr = nullptr;
    }
    else if (m_info_level == 1) // medium information
    {
        dump_type = static_cast<MINIDUMP_TYPE>(
            MiniDumpWithPrivateReadWriteMemory | MiniDumpIgnoreInaccessibleMemory |
            MiniDumpWithDataSegs | MiniDumpWithHandleData | MiniDumpWithFullMemoryInfo |
            MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);

        callback_info.CallbackRoutine = (MINIDUMP_CALLBACK_ROUTINE)mini_dump_callback;
        callback_info.CallbackParam = this;
        callback_info_ptr = &callback_info;
    }
    else if (m_info_level == 2) // Maximal information
    {
        dump_type = static_cast<MINIDUMP_TYPE>(
            MiniDumpWithFullMemory | MiniDumpWithFullMemoryInfo | MiniDumpWithHandleData |
            MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules |
            MiniDumpWithIndirectlyReferencedMemory);
        callback_info_ptr = nullptr;
    }

    SetLastError(0);
    auto process_id = GetProcessId(process);
    BOOL result = m_MiniDumpWriteDump(process, process_id, file, dump_type, &exc_info, nullptr, callback_info_ptr);
    if (!result) {
        char buf[256];
        sprintf(buf, "MiniDumpWriteDump %lu %lu %p failed with error %lu", process_id, thread_id, file, GetLastError());
        CRASHHANDLER_ERR(buf); // ERROR_INVALID_PARAMETER?
    }

    CloseHandle(file);
    return result != FALSE;
}

MiniDumpHelper::MiniDumpHelper()
{
    m_dbghelp_lib = LoadLibraryW(L"Dbghelp.dll");
    if (m_dbghelp_lib) {
        // Note: double cast is needed to fix cast-function-type GCC warning
        m_MiniDumpWriteDump = reinterpret_cast<MiniDumpWriteDump_Type>(reinterpret_cast<void(*)()>(
            GetProcAddress(m_dbghelp_lib, "MiniDumpWriteDump")));
    }
}

MiniDumpHelper::~MiniDumpHelper()
{
    FreeLibrary(m_dbghelp_lib);
    m_MiniDumpWriteDump = nullptr;
}
