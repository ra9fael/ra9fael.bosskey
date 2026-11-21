// The host starts this fixed helper; only its input-hook mode requests UAC.
#include "../input/GuardProtocol.h"
#include "../input/KeyboardHook.h"

#include <shellapi.h>
#include <objbase.h>
#include <cwchar>

namespace {
using namespace bosskey;
using namespace bosskey::guard;

struct Options {
    std::wstring session, sequence;
    DWORD owner = 0;
    bool elevated = false;
};

Handle connect(const Options& options)
{
    Handle pipe(::CreateFileW(pipeName(options.session).c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr));
    if (!pipe)
        return Handle();
    ULONG server = 0;
    if (!::GetNamedPipeServerProcessId(pipe.get(), &server) || server != options.owner)
        return Handle();
    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!::SetNamedPipeHandleState(pipe.get(), &mode, nullptr, nullptr))
        return Handle();
    return pipe;
}

int runGuard(const Options& options, HANDLE owner)
{
    Handle pipe = connect(options);
    if (!pipe)
        return 3;
    if (!elevated(::GetCurrentProcess())) {
        reportFailure(pipe.get(), ERROR_ELEVATION_REQUIRED);
        return 4;
    }
    Handle exitEvent(::CreateEventW(nullptr, TRUE, FALSE, nullptr));
    std::string sequence;
    for (wchar_t c : options.sequence)
        sequence.push_back(static_cast<char>(c)); // validSequence restricts this to ASCII
    KeyboardHook hook(sequence, [&] { ::SetEvent(exitEvent.get()); });
    const DWORD error = hook.install();
    if (error) {
        reportFailure(pipe.get(), error);
        return 5;
    }
    if (!write(pipe.get(), Kind::Ready))
        return 6;
    bool exitSent = false;
    {
        Packet packet {};
        Read input(pipe.get());
        bool connected = input.begin(packet);
        while (connected) {
            const HANDLE events[] = {owner, input.event(), exitEvent.get()};
            const DWORD which = ::MsgWaitForMultipleObjects(3, events, FALSE, INFINITE, QS_ALLINPUT);
            if (which == WAIT_OBJECT_0 || which == WAIT_FAILED)
                break;
            if (which == WAIT_OBJECT_0 + 1) {
                // Stop is the entire command interface; unknown packets end it.
                input.finish();
                break;
            }
            if (which == WAIT_OBJECT_0 + 2) {
                ::ResetEvent(exitEvent.get());
                if (!exitSent) {
                    exitSent = true;
                    connected = write(pipe.get(), Kind::Exit);
                }
            }
            if (which == WAIT_OBJECT_0 + 3) {
                MSG message;
                while (::PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                    if (message.message == WM_QUIT) {
                        connected = false;
                        break;
                    }
                    ::TranslateMessage(&message);
                    ::DispatchMessageW(&message);
                }
            }
        }
    }
    drainKeyboard(hook);
    write(pipe.get(), Kind::Stopped);
    return 0;
}

int run(int argc, wchar_t** argv)
{
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::wstring arg(argv[i]);
        if (arg == L"--elevated")
            options.elevated = true;
        else if (arg == L"--session" && i + 1 < argc)
            options.session = argv[++i];
        else if (arg == L"--sequence" && i + 1 < argc)
            options.sequence = argv[++i];
        else if (arg == L"--owner" && i + 1 < argc) {
            wchar_t* end = nullptr;
            options.owner = ::wcstoul(argv[++i], &end, 10);
            if (!end || *end)
                return 1;
        } else
            return 1;
    }
    if (!validSession(options.session) || !validSequence(options.sequence) || !options.owner)
        return 1;
    std::wstring self(32768, L'\0');
    const DWORD length = ::GetModuleFileNameW(nullptr, self.data(), static_cast<DWORD>(self.size()));
    if (!length || length >= self.size())
        return 2;
    self.resize(length);
    const std::wstring folder = self.substr(0, self.find_last_of(L"\\/"));
    Handle owner(::OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, options.owner));
    if (!owner)
        return 2;
    std::wstring image(32768, L'\0');
    DWORD size = static_cast<DWORD>(image.size());
    if (!::QueryFullProcessImageNameW(owner.get(), 0, image.data(), &size))
        return 2;
    image.resize(size);
    if (::_wcsicmp(image.c_str(), (folder + L"\\bosskey.exe").c_str()) != 0)
        return 2;
    if (options.elevated || elevated(::GetCurrentProcess()))
        return runGuard(options, owner.get());

    const std::wstring args = L"--elevated --session " + options.session + L" --owner "
        + std::to_wstring(options.owner) + L" --sequence " + options.sequence;
    SHELLEXECUTEINFOW info {};
    info.cbSize = sizeof info;
    info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    info.lpVerb = L"runas";
    info.lpFile = self.c_str();
    info.lpParameters = args.c_str();
    info.lpDirectory = folder.c_str();
    info.nShow = SW_HIDE;
    if (!::ShellExecuteExW(&info)) {
        const DWORD error = ::GetLastError();
        Handle pipe = connect(options);
        if (pipe)
            reportFailure(pipe.get(), error);
        return error == ERROR_CANCELLED ? 0 : 7;
    }
    Handle child(info.hProcess);
    return 0;
}
} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    ::SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
    const HRESULT com = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    int argc = 0;
    wchar_t** argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
    const int result = argv ? run(argc, argv) : 1;
    ::LocalFree(argv);
    if (SUCCEEDED(com))
        ::CoUninitialize();
    return result;
}
