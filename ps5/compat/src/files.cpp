// Super Mario Galaxy for the PlayStation 5 - files, directories, file mappings,
// the environment and strings, on the console's FreeBSD libc. Paths arrive as
// Windows writes them (UTF-16 on Windows, UTF-32 here, '\' separators) or as
// std::filesystem's UTF-8; both become the console's. A relative path is the
// title folder's (ps5/src/paths.c wraps the libc calls below).
//
// SPDX-License-Identifier: GPL-3.0-only
#include "compat_internal.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <cwctype>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace compat {

// ---- strings ----------------------------------------------------------------------
std::string utf8(std::wstring_view text) {
    std::string out;
    out.reserve(text.size());
    for (const wchar_t wc : text) {
        const auto c = static_cast<std::uint32_t>(wc);
        if (c < 0x80) {
            out.push_back(static_cast<char>(c));
        } else if (c < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (c >> 6)));
            out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        } else if (c < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (c >> 12)));
            out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (c >> 18)));
            out.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        }
    }
    return out;
}

std::string utf8(const wchar_t* text) { return text != nullptr ? utf8(std::wstring_view(text)) : std::string{}; }

std::wstring wide(std::string_view text) {
    std::wstring out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size();) {
        const auto c = static_cast<unsigned char>(text[i]);
        std::uint32_t code = c;
        std::size_t extra = 0;
        if (c >= 0xF0) {
            code = c & 0x07;
            extra = 3;
        } else if (c >= 0xE0) {
            code = c & 0x0F;
            extra = 2;
        } else if (c >= 0xC0) {
            code = c & 0x1F;
            extra = 1;
        }
        ++i;
        for (std::size_t k = 0; k < extra && i < text.size(); ++k, ++i) {
            code = (code << 6) | (static_cast<unsigned char>(text[i]) & 0x3F);
        }
        out.push_back(static_cast<wchar_t>(code));
    }
    return out;
}

std::string host_path(const char* path) {
    if (path == nullptr) return {};
    std::string out(path);
    for (char& c : out) {
        if (c == '\\') c = '/';
    }
    // "C:/..." has no meaning on the console: the rest is taken as absolute.
    if (out.size() >= 2 && std::isalpha(static_cast<unsigned char>(out[0])) && out[1] == ':') out.erase(0, 2);
    return out;
}

std::string host_path(const wchar_t* path) { return host_path(utf8(path).c_str()); }

DWORD error_from_errno(int error) noexcept {
    switch (error) {
    case 0: return ERROR_SUCCESS;
    case ENOENT: return ERROR_FILE_NOT_FOUND;
    case ENOTDIR: return ERROR_PATH_NOT_FOUND;
    case EACCES:
    case EPERM:
    case EROFS: return ERROR_ACCESS_DENIED;
    case EEXIST: return ERROR_ALREADY_EXISTS;
    case EBADF: return ERROR_INVALID_HANDLE;
    case ENOMEM: return ERROR_NOT_ENOUGH_MEMORY;
    case ENOSPC: return ERROR_DISK_FULL;
    case ENOTEMPTY: return ERROR_DIR_NOT_EMPTY;
    case EINVAL: return ERROR_INVALID_PARAMETER;
    case EBUSY: return ERROR_BUSY;
    case EMFILE:
    case ENFILE: return ERROR_TOO_MANY_OPEN_FILES;
    case ENAMETOOLONG: return ERROR_INVALID_NAME;
    case ENOSYS:
    case ENOTSUP: return ERROR_NOT_SUPPORTED;
    default: return ERROR_GEN_FAILURE;
    }
}

wchar_t* wide_duplicate(std::wstring_view text) noexcept {
    auto* copy = static_cast<wchar_t*>(std::malloc((text.size() + 1) * sizeof(wchar_t)));
    if (copy == nullptr) return nullptr;
    std::wmemcpy(copy, text.data(), text.size());
    copy[text.size()] = L'\0';
    return copy;
}

DWORD copy_out(std::wstring_view text, LPWSTR buffer, DWORD size) noexcept {
    if (buffer == nullptr || size <= text.size()) return static_cast<DWORD>(text.size() + 1);
    std::wmemcpy(buffer, text.data(), text.size());
    buffer[text.size()] = L'\0';
    return static_cast<DWORD>(text.size());
}

namespace {
void set_errno_error() noexcept { SetLastError(error_from_errno(errno)); }

// GALAXY_PS5_TRACE_FILES=1: every file call that fails, with its path and error
bool tracing_files() noexcept {
    static const bool enabled = [] {
        const char* value = std::getenv("GALAXY_PS5_TRACE_FILES");
        return value != nullptr && value[0] == '1';
    }();
    return enabled;
}
void trace_failure(const char* operation, const std::string& path, DWORD extra = 0) noexcept {
    if (!tracing_files()) return;
    std::fprintf(stderr, "[files] %s %s -> error %u errno %d (%u)\n", operation, path.c_str(), GetLastError(), errno, extra);
}

// 100 ns units since 1601, from a POSIX time
FILETIME filetime_from(const struct timespec& time) noexcept {
    const std::uint64_t units = static_cast<std::uint64_t>(time.tv_sec) * 10000000ull +
                                static_cast<std::uint64_t>(time.tv_nsec) / 100u + 116444736000000000ull;
    return FILETIME{static_cast<DWORD>(units), static_cast<DWORD>(units >> 32)};
}

DWORD attributes_from(const struct stat& status) noexcept {
    if (S_ISDIR(status.st_mode)) return FILE_ATTRIBUTE_DIRECTORY;
    return (status.st_mode & S_IWUSR) != 0 ? FILE_ATTRIBUTE_NORMAL : FILE_ATTRIBUTE_READONLY;
}

struct File final : Object {
    explicit File(int descriptor, std::string file_path)
        : Object(Kind::File), fd(descriptor), path(std::move(file_path)) {}
    ~File() override {
        if (fd >= 0) ::close(fd);
        if (delete_on_close) ::unlink(path.c_str());
    }
    int fd;
    std::string path;
    bool delete_on_close = false;
    bool append = false;
};

File* file_of(HANDLE handle) noexcept {
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE) return nullptr;
    auto* object = static_cast<Object*>(handle);
    return object->kind == Kind::File ? static_cast<File*>(object) : nullptr;
}

// ---- named streams ---------------------------------------------------------------
// NTFS alternate data streams ("file:stream"), which Nebula's NAND keeps each
// save file's Wii metadata in. UFS has none: a stream is a file in a shadow
// folder outside the tree it belongs to, /app0/userdata/.streams/<path>@/<name>,
// so a scan of the NAND folder never sees it. Streams move, and are deleted,
// with their file; a newly created file or folder starts with none.
constexpr const char* kStreamsRoot = "/app0/userdata/.streams";
bool make_directories(const std::string& path);

struct StreamPath {
    std::string base;
    std::string name;  // empty: the file itself
};

StreamPath split_stream(const std::string& path) {
    const auto slash = path.find_last_of('/');
    const std::size_t start = slash == std::string::npos ? 0 : slash + 1;
    const auto colon = path.find(':', start);
    if (colon == std::string::npos) return {path, {}};
    StreamPath result{path.substr(0, colon), path.substr(colon + 1)};
    if (const auto type = result.name.find(':'); type != std::string::npos) result.name.resize(type);  // ":$DATA"
    return result;
}

std::string absolute_path(const std::string& path) {
    if (!path.empty() && path[0] == '/') return path;
    std::string relative = path;
    while (relative.rfind("./", 0) == 0) relative.erase(0, 2);
    return "/app0/" + relative;
}

std::string shadow_of(const std::string& base) { return std::string(kStreamsRoot) + absolute_path(base) + "@"; }
std::string shadow_children_of(const std::string& directory) { return std::string(kStreamsRoot) + absolute_path(directory); }

// The file a path names on disk: a stream's shadow file, or the path itself.
std::string on_disk(const std::string& path) {
    const StreamPath stream = split_stream(path);
    return stream.name.empty() ? stream.base : shadow_of(stream.base) + "/" + stream.name;
}

void remove_tree(const std::string& path) {
    struct stat status{};
    if (::lstat(path.c_str(), &status) != 0) return;
    if (S_ISDIR(status.st_mode)) {
        if (DIR* directory = ::opendir(path.c_str())) {
            while (struct dirent* entry = ::readdir(directory)) {
                if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) continue;
                remove_tree(path + "/" + entry->d_name);
            }
            ::closedir(directory);
        }
        ::rmdir(path.c_str());
    } else {
        ::unlink(path.c_str());
    }
}

void move_shadow(const std::string& from, const std::string& to) {
    struct stat status{};
    if (::stat(from.c_str(), &status) != 0) return;
    remove_tree(to);
    const auto slash = to.find_last_of('/');
    if (slash != std::string::npos) make_directories(to.substr(0, slash));
    ::rename(from.c_str(), to.c_str());
}

// A file or folder moved: its streams, and (a folder) those of what it holds.
void move_streams_with(const std::string& from, const std::string& to) {
    move_shadow(shadow_of(from), shadow_of(to));
    move_shadow(shadow_children_of(from), shadow_children_of(to));
}

void forget_streams_of(const std::string& base) {
    remove_tree(shadow_of(base));
    remove_tree(shadow_children_of(base));
}

HANDLE open_file(const std::string& requested, DWORD access, DWORD disposition, DWORD flags) noexcept {
    const StreamPath stream = split_stream(requested);
    const std::string path = on_disk(requested);
    int mode = 0;
    const bool read = (access & (GENERIC_READ | GENERIC_ALL | FILE_READ_DATA | FILE_GENERIC_READ)) != 0;
    const bool write = (access & (GENERIC_WRITE | GENERIC_ALL | FILE_WRITE_DATA | FILE_APPEND_DATA |
                                  FILE_GENERIC_WRITE | DELETE)) != 0;
    if (read && write) {
        mode = O_RDWR;
    } else if (write) {
        mode = O_WRONLY;
    } else {
        mode = O_RDONLY;
    }
    switch (disposition) {
    case CREATE_NEW: mode |= O_CREAT | O_EXCL; break;
    case CREATE_ALWAYS: mode |= O_CREAT | O_TRUNC; break;
    case OPEN_ALWAYS: mode |= O_CREAT; break;
    case TRUNCATE_EXISTING: mode |= O_TRUNC; break;
    default: break;
    }
    struct stat status{};
    const bool existed = ::stat(path.c_str(), &status) == 0;
    const bool creates = disposition == CREATE_NEW || disposition == CREATE_ALWAYS || disposition == OPEN_ALWAYS;
    if (!stream.name.empty()) {
        // A stream of a file that does not exist: Windows creates the file too.
        struct stat base_status{};
        if (::stat(stream.base.c_str(), &base_status) != 0) {
            if (!creates) {
                SetLastError(ERROR_FILE_NOT_FOUND);
                trace_failure("open-stream-of-missing", requested);
                return INVALID_HANDLE_VALUE;
            }
            const int base_fd = ::open(stream.base.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0666);
            if (base_fd < 0) {
                set_errno_error();
                return INVALID_HANDLE_VALUE;
            }
            ::close(base_fd);
        }
        if (creates) make_directories(shadow_of(stream.base));
    } else if (creates && !existed) {
        forget_streams_of(path);  // a new file has no streams
    }
    if (existed && S_ISDIR(status.st_mode)) {
        // A directory opened for its attributes (FILE_FLAG_BACKUP_SEMANTICS)
        mode = O_RDONLY;
    }
    const int fd = ::open(path.c_str(), mode | O_CLOEXEC, 0666);
    if (fd < 0) {
        set_errno_error();
        trace_failure("open", path, disposition);
        return INVALID_HANDLE_VALUE;
    }
    auto* file = new File(fd, path);
    file->delete_on_close = (flags & FILE_FLAG_DELETE_ON_CLOSE) != 0;
    file->append = (access & FILE_APPEND_DATA) != 0 && (access & FILE_WRITE_DATA) == 0;
    // OPEN_ALWAYS/CREATE_ALWAYS say whether the file was there before.
    SetLastError((disposition == OPEN_ALWAYS || disposition == CREATE_ALWAYS) && existed ? ERROR_ALREADY_EXISTS
                                                                                        : ERROR_SUCCESS);
    return file;
}

BOOL transfer(HANDLE handle, void* buffer, DWORD bytes, LPDWORD done, LPOVERLAPPED overlapped, bool write) noexcept {
    File* file = file_of(handle);
    if (done != nullptr) *done = 0;
    if (file == nullptr) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    std::size_t total = 0;
    auto* bytes_pointer = static_cast<char*>(buffer);
    if (overlapped != nullptr) {
        // Overlapped I/O completes at once: a synchronous completion is one of
        // the outcomes Windows documents, and the caller reads the result the same way.
        off_t offset = static_cast<off_t>((static_cast<std::uint64_t>(overlapped->OffsetHigh) << 32) | overlapped->Offset);
        while (total < bytes) {
            const ssize_t n = write ? ::pwrite(file->fd, bytes_pointer + total, bytes - total, offset)
                                    : ::pread(file->fd, bytes_pointer + total, bytes - total, offset);
            if (n < 0) {
                if (errno == EINTR) continue;
                overlapped->Internal = static_cast<ULONG_PTR>(error_from_errno(errno));
                overlapped->InternalHigh = total;
                set_errno_error();
                return FALSE;
            }
            if (n == 0) break;
            total += static_cast<std::size_t>(n);
            offset += n;
        }
        overlapped->Internal = 0;
        overlapped->InternalHigh = total;
        if (overlapped->hEvent != nullptr) SetEvent(overlapped->hEvent);
    } else {
        if (write && file->append) ::lseek(file->fd, 0, SEEK_END);
        while (total < bytes) {
            const ssize_t n = write ? ::write(file->fd, bytes_pointer + total, bytes - total)
                                    : ::read(file->fd, bytes_pointer + total, bytes - total);
            if (n < 0) {
                if (errno == EINTR) continue;
                set_errno_error();
                if (done != nullptr) *done = static_cast<DWORD>(total);
                return FALSE;
            }
            if (n == 0) break;
            total += static_cast<std::size_t>(n);
        }
    }
    if (done != nullptr) *done = static_cast<DWORD>(total);
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

bool make_directories(const std::string& path) {
    std::string partial;
    for (std::size_t i = 0; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == '/') {
            if (!partial.empty() && ::mkdir(partial.c_str(), 0777) != 0 && errno != EEXIST) return false;
        }
        if (i < path.size()) partial.push_back(path[i]);
    }
    return true;
}
}  // namespace

}  // namespace compat

using namespace compat;

// ---- files ----------------------------------------------------------------------------
HANDLE CreateFileW(LPCWSTR name, DWORD access, DWORD, LPSECURITY_ATTRIBUTES, DWORD disposition, DWORD flags,
                   HANDLE) noexcept {
    return open_file(host_path(name), access, disposition, flags);
}
HANDLE CreateFileW(const char* name, DWORD access, DWORD, LPSECURITY_ATTRIBUTES, DWORD disposition, DWORD flags,
                   HANDLE) noexcept {
    return open_file(host_path(name), access, disposition, flags);
}
HANDLE CreateFileA(LPCSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES attributes, DWORD disposition,
                   DWORD flags, HANDLE template_file) noexcept {
    return CreateFileW(name, access, share, attributes, disposition, flags, template_file);
}
BOOL ReadFile(HANDLE file, LPVOID buffer, DWORD bytes, LPDWORD read, LPOVERLAPPED overlapped) noexcept {
    return transfer(file, buffer, bytes, read, overlapped, false);
}
BOOL WriteFile(HANDLE file, LPCVOID buffer, DWORD bytes, LPDWORD written, LPOVERLAPPED overlapped) noexcept {
    return transfer(file, const_cast<void*>(buffer), bytes, written, overlapped, true);
}
BOOL GetOverlappedResult(HANDLE, LPOVERLAPPED overlapped, LPDWORD transferred, BOOL) noexcept {
    if (overlapped == nullptr) return FALSE;
    if (transferred != nullptr) *transferred = static_cast<DWORD>(overlapped->InternalHigh);
    if (overlapped->Internal != 0) {
        SetLastError(static_cast<DWORD>(overlapped->Internal));
        return FALSE;
    }
    return TRUE;
}
BOOL GetOverlappedResultEx(HANDLE file, LPOVERLAPPED overlapped, LPDWORD transferred, DWORD, BOOL) noexcept {
    return GetOverlappedResult(file, overlapped, transferred, TRUE);
}
BOOL CancelIo(HANDLE) noexcept { return TRUE; }
BOOL CancelIoEx(HANDLE, LPOVERLAPPED) noexcept {
    SetLastError(ERROR_NOT_FOUND);
    return FALSE;
}
BOOL FlushFileBuffers(HANDLE handle) noexcept {
    File* file = file_of(handle);
    if (file == nullptr) return FALSE;
    ::fsync(file->fd);
    return TRUE;
}
BOOL GetFileSizeEx(HANDLE handle, PLARGE_INTEGER size) noexcept {
    File* file = file_of(handle);
    struct stat status{};
    if (file == nullptr || size == nullptr || ::fstat(file->fd, &status) != 0) return FALSE;
    size->QuadPart = status.st_size;
    return TRUE;
}
DWORD GetFileSize(HANDLE handle, LPDWORD high) noexcept {
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(handle, &size)) return INVALID_FILE_SIZE;
    if (high != nullptr) *high = static_cast<DWORD>(size.HighPart);
    return size.LowPart;
}
BOOL SetFilePointerEx(HANDLE handle, LARGE_INTEGER distance, PLARGE_INTEGER position, DWORD method) noexcept {
    File* file = file_of(handle);
    if (file == nullptr) return FALSE;
    const int whence = method == FILE_BEGIN ? SEEK_SET : method == FILE_CURRENT ? SEEK_CUR : SEEK_END;
    const off_t result = ::lseek(file->fd, distance.QuadPart, whence);
    if (result < 0) {
        set_errno_error();
        return FALSE;
    }
    if (position != nullptr) position->QuadPart = result;
    return TRUE;
}
DWORD SetFilePointer(HANDLE handle, LONG distance, PLONG high, DWORD method) noexcept {
    LARGE_INTEGER move{};
    move.QuadPart = high != nullptr ? (static_cast<LONGLONG>(*high) << 32) | static_cast<DWORD>(distance) : distance;
    LARGE_INTEGER position{};
    if (!SetFilePointerEx(handle, move, &position, method)) return INVALID_SET_FILE_POINTER;
    if (high != nullptr) *high = position.HighPart;
    return position.LowPart;
}
BOOL SetEndOfFile(HANDLE handle) noexcept {
    File* file = file_of(handle);
    if (file == nullptr) return FALSE;
    const off_t position = ::lseek(file->fd, 0, SEEK_CUR);
    return ::ftruncate(file->fd, position) == 0;
}
DWORD GetFileType(HANDLE handle) noexcept { return file_of(handle) != nullptr ? FILE_TYPE_DISK : FILE_TYPE_UNKNOWN; }
BOOL SetFileTime(HANDLE, const FILETIME*, const FILETIME*, const FILETIME*) noexcept { return TRUE; }
BOOL GetFileTime(HANDLE handle, LPFILETIME creation, LPFILETIME access, LPFILETIME write) noexcept {
    File* file = file_of(handle);
    struct stat status{};
    if (file == nullptr || ::fstat(file->fd, &status) != 0) return FALSE;
    if (creation != nullptr) *creation = filetime_from(status.st_mtim);
    if (access != nullptr) *access = filetime_from(status.st_atim);
    if (write != nullptr) *write = filetime_from(status.st_mtim);
    return TRUE;
}
static DWORD file_attributes(const std::string& requested) noexcept {
    const std::string path = on_disk(requested);
    struct stat status{};
    if (::stat(path.c_str(), &status) != 0) {
        SetLastError(error_from_errno(errno));
        if (errno != ENOENT) trace_failure("attributes", path);
        return INVALID_FILE_ATTRIBUTES;
    }
    return attributes_from(status);
}
DWORD GetFileAttributesW(LPCWSTR name) noexcept { return file_attributes(host_path(name)); }
DWORD GetFileAttributesW(const char* name) noexcept { return file_attributes(host_path(name)); }
DWORD GetFileAttributesA(LPCSTR name) noexcept { return file_attributes(host_path(name)); }
BOOL SetFileAttributesW(LPCWSTR, DWORD) noexcept { return TRUE; }
BOOL SetFileAttributesW(const char*, DWORD) noexcept { return TRUE; }
static BOOL file_attributes_ex(const std::string& requested, LPVOID information) noexcept {
    const std::string path = on_disk(requested);
    struct stat status{};
    if (information == nullptr || ::stat(path.c_str(), &status) != 0) {
        SetLastError(error_from_errno(errno));
        return FALSE;
    }
    auto* data = static_cast<WIN32_FILE_ATTRIBUTE_DATA*>(information);
    data->dwFileAttributes = attributes_from(status);
    data->ftCreationTime = filetime_from(status.st_mtim);
    data->ftLastAccessTime = filetime_from(status.st_atim);
    data->ftLastWriteTime = filetime_from(status.st_mtim);
    data->nFileSizeHigh = static_cast<DWORD>(static_cast<std::uint64_t>(status.st_size) >> 32);
    data->nFileSizeLow = static_cast<DWORD>(status.st_size);
    return TRUE;
}
BOOL GetFileAttributesExW(LPCWSTR name, GET_FILEEX_INFO_LEVELS, LPVOID information) noexcept {
    return file_attributes_ex(host_path(name), information);
}
BOOL GetFileAttributesExW(const char* name, GET_FILEEX_INFO_LEVELS, LPVOID information) noexcept {
    return file_attributes_ex(host_path(name), information);
}
BOOL GetFileInformationByHandle(HANDLE handle, LPBY_HANDLE_FILE_INFORMATION information) noexcept {
    File* file = file_of(handle);
    struct stat status{};
    if (file == nullptr || information == nullptr || ::fstat(file->fd, &status) != 0) return FALSE;
    *information = {};
    information->dwFileAttributes = attributes_from(status);
    information->ftCreationTime = filetime_from(status.st_mtim);
    information->ftLastAccessTime = filetime_from(status.st_atim);
    information->ftLastWriteTime = filetime_from(status.st_mtim);
    information->dwVolumeSerialNumber = static_cast<DWORD>(status.st_dev);
    information->nFileSizeHigh = static_cast<DWORD>(static_cast<std::uint64_t>(status.st_size) >> 32);
    information->nFileSizeLow = static_cast<DWORD>(status.st_size);
    // NTFS gives a folder one link; UFS counts "." and each child's "..". The
    // console's /data reports no links for a file at all: a file has one.
    information->nNumberOfLinks =
        S_ISDIR(status.st_mode) || status.st_nlink == 0 ? 1u : static_cast<DWORD>(status.st_nlink);
    if (!S_ISDIR(status.st_mode) && status.st_nlink != 1) {
        trace_failure("nlink", file->path, static_cast<DWORD>(status.st_nlink));
    }
    information->nFileIndexHigh = static_cast<DWORD>(static_cast<std::uint64_t>(status.st_ino) >> 32);
    information->nFileIndexLow = static_cast<DWORD>(status.st_ino);
    return TRUE;
}
BOOL GetFileInformationByHandleEx(HANDLE handle, FILE_INFO_BY_HANDLE_CLASS info_class, LPVOID information,
                                  DWORD size) noexcept {
    File* file = file_of(handle);
    struct stat status{};
    if (file == nullptr || information == nullptr || ::fstat(file->fd, &status) != 0) return FALSE;
    switch (info_class) {
    case FileStandardInfo: {
        if (size < sizeof(FILE_STANDARD_INFO)) break;
        auto* info = static_cast<FILE_STANDARD_INFO*>(information);
        *info = {};
        info->AllocationSize.QuadPart = status.st_blocks * 512;
        info->EndOfFile.QuadPart = status.st_size;
        info->NumberOfLinks = S_ISDIR(status.st_mode) || status.st_nlink == 0 ? 1u : static_cast<DWORD>(status.st_nlink);
        info->Directory = S_ISDIR(status.st_mode);
        return TRUE;
    }
    case FileIdInfo: {
        if (size < sizeof(FILE_ID_INFO)) break;
        auto* info = static_cast<FILE_ID_INFO*>(information);
        *info = {};
        info->VolumeSerialNumber = static_cast<ULONGLONG>(status.st_dev);
        std::memcpy(info->FileId.Identifier, &status.st_ino, sizeof(status.st_ino));
        return TRUE;
    }
    case FileCaseSensitiveInfo: {
        if (size < sizeof(FILE_CASE_SENSITIVE_INFO)) break;
        // Windows' default (Nebula's NAND refuses a case-sensitive folder and
        // checks name collisions itself); the game names its files one way.
        static_cast<FILE_CASE_SENSITIVE_INFO*>(information)->Flags = 0;
        return TRUE;
    }
    default: break;
    }
    SetLastError(ERROR_NOT_SUPPORTED);
    trace_failure("get-info-class", file->path, static_cast<DWORD>(info_class));
    return FALSE;
}
BOOL SetFileInformationByHandle(HANDLE handle, FILE_INFO_BY_HANDLE_CLASS info_class, LPVOID information,
                                DWORD size) noexcept {
    File* file = file_of(handle);
    if (file == nullptr || information == nullptr) return FALSE;
    switch (info_class) {
    case FileDispositionInfo:
        if (size < sizeof(FILE_DISPOSITION_INFO)) break;
        file->delete_on_close = static_cast<FILE_DISPOSITION_INFO*>(information)->DeleteFile != 0;
        return TRUE;
    case FileDispositionInfoEx:
        if (size < sizeof(FILE_DISPOSITION_INFO_EX)) break;
        file->delete_on_close = (static_cast<FILE_DISPOSITION_INFO_EX*>(information)->Flags & FILE_DISPOSITION_FLAG_DELETE) != 0;
        return TRUE;
    case FileEndOfFileInfo:
        if (size < sizeof(FILE_END_OF_FILE_INFO)) break;
        return ::ftruncate(file->fd, static_cast<FILE_END_OF_FILE_INFO*>(information)->EndOfFile.QuadPart) == 0;
    case FileCaseSensitiveInfo: return TRUE;
    case FileRenameInfo:
    case FileRenameInfoEx: {
        auto* info = static_cast<FILE_RENAME_INFO*>(information);
        const std::wstring target(info->FileName, info->FileNameLength / sizeof(WCHAR));
        const std::string to = host_path(target.c_str());
        if (::rename(file->path.c_str(), to.c_str()) != 0) {
            set_errno_error();
            return FALSE;
        }
        move_streams_with(file->path, to);
        file->path = to;
        return TRUE;
    }
    default: break;
    }
    SetLastError(ERROR_NOT_SUPPORTED);
    trace_failure("set-info-class", file->path, static_cast<DWORD>(info_class));
    return FALSE;
}
DWORD GetFinalPathNameByHandleW(HANDLE handle, LPWSTR path, DWORD count, DWORD) noexcept {
    File* file = file_of(handle);
    if (file == nullptr) return 0;
    return copy_out(wide(file->path), path, count);
}
static BOOL delete_file(const std::string& requested) noexcept {
    const StreamPath stream = split_stream(requested);
    const std::string path = on_disk(requested);
    if (::unlink(path.c_str()) != 0) {
        set_errno_error();
        trace_failure("delete", path);
        return FALSE;
    }
    if (stream.name.empty()) forget_streams_of(path);
    return TRUE;
}
BOOL DeleteFileW(LPCWSTR name) noexcept { return delete_file(host_path(name)); }
BOOL DeleteFileW(const char* name) noexcept { return delete_file(host_path(name)); }
BOOL DeleteFileA(LPCSTR name) noexcept { return delete_file(host_path(name)); }
static BOOL move_file(const std::string& requested_from, const std::string& requested_to, DWORD flags) noexcept {
    const bool streams = !split_stream(requested_from).name.empty() || !split_stream(requested_to).name.empty();
    const std::string from = on_disk(requested_from);
    const std::string to = on_disk(requested_to);
    if (streams) {
        const auto slash = to.find_last_of('/');
        if (slash != std::string::npos) make_directories(to.substr(0, slash));
    }
    struct stat status{};
    if ((flags & MOVEFILE_REPLACE_EXISTING) == 0 && ::stat(to.c_str(), &status) == 0) {
        SetLastError(ERROR_ALREADY_EXISTS);
        trace_failure("move-exists", to);
        return FALSE;
    }
    if (::rename(from.c_str(), to.c_str()) != 0) {
        set_errno_error();
        trace_failure("move", from + " -> " + to);
        return FALSE;
    }
    if (!streams) move_streams_with(from, to);
    return TRUE;
}
BOOL MoveFileExW(LPCWSTR from, LPCWSTR to, DWORD flags) noexcept { return move_file(host_path(from), host_path(to), flags); }
BOOL MoveFileExW(const char* from, const char* to, DWORD flags) noexcept {
    return move_file(host_path(from), host_path(to), flags);
}
BOOL MoveFileW(LPCWSTR from, LPCWSTR to) noexcept { return move_file(host_path(from), host_path(to), 0); }
BOOL MoveFileW(const char* from, const char* to) noexcept { return move_file(host_path(from), host_path(to), 0); }
static BOOL replace_file(const std::string& replaced, const std::string& replacement, const std::string& backup) noexcept {
    if (!backup.empty()) {
        // A copy of the old file first, as ReplaceFile keeps one
        if (::link(replaced.c_str(), backup.c_str()) != 0) {
            FILE* in = std::fopen(replaced.c_str(), "rb");
            FILE* out = in != nullptr ? std::fopen(backup.c_str(), "wb") : nullptr;
            char buffer[65536];
            std::size_t n;
            while (out != nullptr && (n = std::fread(buffer, 1, sizeof(buffer), in)) > 0) std::fwrite(buffer, 1, n, out);
            if (out != nullptr) std::fclose(out);
            if (in != nullptr) std::fclose(in);
        }
    }
    if (::rename(replacement.c_str(), replaced.c_str()) != 0) {
        set_errno_error();
        return FALSE;
    }
    move_streams_with(replacement, replaced);
    return TRUE;
}
BOOL ReplaceFileW(LPCWSTR replaced, LPCWSTR replacement, LPCWSTR backup, DWORD, LPVOID, LPVOID) noexcept {
    return replace_file(host_path(replaced), host_path(replacement), backup != nullptr ? host_path(backup) : "");
}
BOOL ReplaceFileW(const char* replaced, const char* replacement, const char* backup, DWORD, LPVOID, LPVOID) noexcept {
    return replace_file(host_path(replaced), host_path(replacement), backup != nullptr ? host_path(backup) : "");
}
static BOOL copy_file(const std::string& from, const std::string& to, BOOL fail_if_exists) noexcept {
    struct stat status{};
    if (fail_if_exists && ::stat(to.c_str(), &status) == 0) {
        SetLastError(ERROR_FILE_EXISTS);
        return FALSE;
    }
    FILE* in = std::fopen(from.c_str(), "rb");
    if (in == nullptr) {
        set_errno_error();
        return FALSE;
    }
    FILE* out = std::fopen(to.c_str(), "wb");
    if (out == nullptr) {
        set_errno_error();
        std::fclose(in);
        return FALSE;
    }
    char buffer[65536];
    std::size_t n;
    bool ok = true;
    while ((n = std::fread(buffer, 1, sizeof(buffer), in)) > 0) ok = ok && std::fwrite(buffer, 1, n, out) == n;
    std::fclose(in);
    ok = std::fclose(out) == 0 && ok;
    return ok ? TRUE : FALSE;
}
BOOL CopyFileW(LPCWSTR from, LPCWSTR to, BOOL fail_if_exists) noexcept {
    return copy_file(host_path(from), host_path(to), fail_if_exists);
}
BOOL CopyFileW(const char* from, const char* to, BOOL fail_if_exists) noexcept {
    return copy_file(host_path(from), host_path(to), fail_if_exists);
}
static BOOL hard_link(const std::string& link, const std::string& existing) noexcept {
    if (::link(existing.c_str(), link.c_str()) != 0) {
        set_errno_error();
        trace_failure("hardlink", existing + " -> " + link);
        return FALSE;
    }
    return TRUE;
}
BOOL CreateHardLinkW(LPCWSTR link, LPCWSTR existing, LPSECURITY_ATTRIBUTES) noexcept {
    return hard_link(host_path(link), host_path(existing));
}
BOOL CreateHardLinkW(const char* link, const char* existing, LPSECURITY_ATTRIBUTES) noexcept {
    return hard_link(host_path(link), host_path(existing));
}
static BOOL create_directory(const std::string& path) noexcept {
    if (::mkdir(path.c_str(), 0777) != 0) {
        set_errno_error();
        trace_failure("mkdir", path);
        return FALSE;
    }
    forget_streams_of(path);
    return TRUE;
}
BOOL CreateDirectoryW(LPCWSTR name, LPSECURITY_ATTRIBUTES) noexcept { return create_directory(host_path(name)); }
BOOL CreateDirectoryW(const char* name, LPSECURITY_ATTRIBUTES) noexcept { return create_directory(host_path(name)); }
BOOL CreateDirectoryA(LPCSTR name, LPSECURITY_ATTRIBUTES) noexcept { return create_directory(host_path(name)); }
static BOOL remove_directory(const std::string& path) noexcept {
    if (::rmdir(path.c_str()) != 0) {
        set_errno_error();
        trace_failure("rmdir", path);
        return FALSE;
    }
    forget_streams_of(path);
    return TRUE;
}
BOOL RemoveDirectoryW(LPCWSTR name) noexcept { return remove_directory(host_path(name)); }
BOOL RemoveDirectoryW(const char* name) noexcept { return remove_directory(host_path(name)); }
int SHCreateDirectoryExW(HWND, LPCWSTR path, const SECURITY_ATTRIBUTES*) noexcept {
    return make_directories(host_path(path)) ? ERROR_SUCCESS : static_cast<int>(error_from_errno(errno));
}
DWORD GetFullPathNameW(LPCWSTR name, DWORD count, LPWSTR buffer, LPWSTR* file_part) noexcept {
    std::string path = host_path(name);
    if (path.empty() || path[0] != '/') path = "/app0/" + path;
    const std::wstring full = wide(path);
    const DWORD result = copy_out(full, buffer, count);
    if (file_part != nullptr && buffer != nullptr && result < count) {
        const auto slash = full.find_last_of(L'/');
        *file_part = slash == std::wstring::npos ? buffer : buffer + slash + 1;
    }
    return result;
}
DWORD GetTempPathW(DWORD count, LPWSTR buffer) noexcept { return copy_out(L"/app0/temp/", buffer, count); }
DWORD GetCurrentDirectoryW(DWORD count, LPWSTR buffer) noexcept { return copy_out(L"/app0", buffer, count); }
BOOL SetCurrentDirectoryW(LPCWSTR) noexcept { return TRUE; }
BOOL GetVolumePathNameW(LPCWSTR, LPWSTR volume, DWORD count) noexcept { return copy_out(L"/", volume, count) < count; }
BOOL GetVolumePathNameW(const char*, LPWSTR volume, DWORD count) noexcept {
    return copy_out(L"/", volume, count) < count;
}
BOOL GetVolumeInformationW(LPCWSTR, LPWSTR name, DWORD name_size, LPDWORD serial, LPDWORD max_component,
                           LPDWORD flags, LPWSTR file_system, DWORD file_system_size) noexcept {
    if (name != nullptr) copy_out(L"app0", name, name_size);
    if (serial != nullptr) *serial = 0x50533500u;
    if (max_component != nullptr) *max_component = 255;
    // Case-sensitive POSIX names; named streams through the shadow folders (above)
    if (flags != nullptr) {
        *flags = FILE_CASE_SENSITIVE_SEARCH | FILE_CASE_PRESERVED_NAMES | FILE_UNICODE_ON_DISK | FILE_NAMED_STREAMS;
    }
    if (file_system != nullptr) copy_out(L"UFS", file_system, file_system_size);
    return TRUE;
}
UINT GetDriveTypeW(LPCWSTR) noexcept { return DRIVE_FIXED; }
static BOOL disk_free(PULARGE_INTEGER available, PULARGE_INTEGER total, PULARGE_INTEGER free) noexcept {
    const ULONGLONG plenty = 64ull << 30;
    if (available != nullptr) available->QuadPart = plenty;
    if (total != nullptr) total->QuadPart = plenty * 4;
    if (free != nullptr) free->QuadPart = plenty;
    return TRUE;
}
BOOL GetDiskFreeSpaceExW(LPCWSTR, PULARGE_INTEGER available, PULARGE_INTEGER total, PULARGE_INTEGER free) noexcept {
    return disk_free(available, total, free);
}
BOOL GetDiskFreeSpaceExW(const char*, PULARGE_INTEGER available, PULARGE_INTEGER total, PULARGE_INTEGER free) noexcept {
    return disk_free(available, total, free);
}

// ---- directory listings ---------------------------------------------------------------
namespace {
struct Find final : Object {
    Find() : Object(Kind::Find) {}
    ~Find() override {
        if (directory != nullptr) ::closedir(directory);
    }
    DIR* directory = nullptr;
    std::string folder;
    std::string pattern;
};

// Windows' wildcards: '*' any run, '?' one character (the console's libc has no fnmatch).
bool wildcard_match(const char* pattern, const char* name) {
    if (*pattern == '\0') return *name == '\0';
    if (*pattern == '*') return wildcard_match(pattern + 1, name) || (*name != '\0' && wildcard_match(pattern, name + 1));
    if (*name == '\0') return false;
    return (*pattern == '?' || *pattern == *name) && wildcard_match(pattern + 1, name + 1);
}

bool next_match(Find* find, LPWIN32_FIND_DATAW data) {
    while (struct dirent* entry = ::readdir(find->directory)) {
        if (!wildcard_match(find->pattern.c_str(), entry->d_name)) continue;
        *data = {};
        struct stat status{};
        const std::string full = find->folder + "/" + entry->d_name;
        if (::stat(full.c_str(), &status) == 0) {
            data->dwFileAttributes = attributes_from(status);
            data->ftCreationTime = filetime_from(status.st_mtim);
            data->ftLastAccessTime = filetime_from(status.st_atim);
            data->ftLastWriteTime = filetime_from(status.st_mtim);
            data->nFileSizeHigh = static_cast<DWORD>(static_cast<std::uint64_t>(status.st_size) >> 32);
            data->nFileSizeLow = static_cast<DWORD>(status.st_size);
        }
        const std::wstring name = wide(entry->d_name);
        const std::size_t length = std::min<std::size_t>(name.size(), MAX_PATH - 1);
        std::wmemcpy(data->cFileName, name.c_str(), length);
        data->cFileName[length] = L'\0';
        return true;
    }
    return false;
}

HANDLE find_first(const std::string& pattern_path, LPWIN32_FIND_DATAW data) {
    auto* find = new Find();
    const auto slash = pattern_path.find_last_of('/');
    find->folder = slash == std::string::npos ? "." : pattern_path.substr(0, slash);
    find->pattern = slash == std::string::npos ? pattern_path : pattern_path.substr(slash + 1);
    if (find->pattern == "*.*") find->pattern = "*";
    find->directory = ::opendir(find->folder.c_str());
    if (find->directory == nullptr || data == nullptr || !next_match(find, data)) {
        SetLastError(find->directory == nullptr ? ERROR_PATH_NOT_FOUND : ERROR_FILE_NOT_FOUND);
        delete find;
        return INVALID_HANDLE_VALUE;
    }
    return find;
}
}  // namespace

HANDLE FindFirstFileW(LPCWSTR pattern, LPWIN32_FIND_DATAW data) noexcept { return find_first(host_path(pattern), data); }
HANDLE FindFirstFileW(const char* pattern, LPWIN32_FIND_DATAW data) noexcept {
    return find_first(host_path(pattern), data);
}
BOOL FindNextFileW(HANDLE handle, LPWIN32_FIND_DATAW data) noexcept {
    auto* find = handle == nullptr || handle == INVALID_HANDLE_VALUE ? nullptr : dynamic_cast<Find*>(static_cast<Object*>(handle));
    if (find == nullptr) return FALSE;
    if (!next_match(find, data)) {
        SetLastError(ERROR_NO_MORE_FILES);
        return FALSE;
    }
    return TRUE;
}
BOOL FindClose(HANDLE handle) noexcept {
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE) return FALSE;
    delete static_cast<Object*>(handle);
    return TRUE;
}
// A file's named streams, from its shadow folder.
namespace {
struct StreamFind final : Object {
    StreamFind() : Object(Kind::Find) {}
    std::vector<std::pair<std::string, std::int64_t>> streams;
    std::size_t next = 0;
};

bool fill_stream(StreamFind* find, LPVOID data) {
    if (find->next >= find->streams.size()) return false;
    auto* out = static_cast<WIN32_FIND_STREAM_DATA*>(data);
    const auto& [name, size] = find->streams[find->next++];
    const std::wstring label = L":" + wide(name) + L":$DATA";
    *out = {};
    out->StreamSize.QuadPart = size;
    std::wmemcpy(out->cStreamName, label.c_str(), std::min<std::size_t>(label.size(), MAX_PATH + 35));
    return true;
}

HANDLE find_first_stream(const std::string& path, LPVOID data) {
    struct stat status{};
    if (::stat(path.c_str(), &status) != 0) {
        SetLastError(ERROR_FILE_NOT_FOUND);
        return INVALID_HANDLE_VALUE;
    }
    auto* find = new StreamFind();
    const std::string shadow = shadow_of(path);
    if (DIR* directory = ::opendir(shadow.c_str())) {
        while (struct dirent* entry = ::readdir(directory)) {
            if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) continue;
            struct stat stream_status{};
            const std::string full = shadow + "/" + entry->d_name;
            if (::stat(full.c_str(), &stream_status) == 0 && S_ISREG(stream_status.st_mode)) {
                find->streams.emplace_back(entry->d_name, static_cast<std::int64_t>(stream_status.st_size));
            }
        }
        ::closedir(directory);
    }
    std::sort(find->streams.begin(), find->streams.end());
    if (data == nullptr || !fill_stream(find, data)) {
        delete find;
        SetLastError(ERROR_HANDLE_EOF);
        return INVALID_HANDLE_VALUE;
    }
    return find;
}
}  // namespace

HANDLE FindFirstStreamW(LPCWSTR name, STREAM_INFO_LEVELS, LPVOID data, DWORD) noexcept {
    return find_first_stream(host_path(name), data);
}
HANDLE FindFirstStreamW(const char* name, STREAM_INFO_LEVELS, LPVOID data, DWORD) noexcept {
    return find_first_stream(host_path(name), data);
}
BOOL FindNextStreamW(HANDLE handle, LPVOID data) noexcept {
    auto* find = dynamic_cast<StreamFind*>(static_cast<Object*>(handle));
    if (find == nullptr || !fill_stream(find, data)) {
        SetLastError(ERROR_HANDLE_EOF);
        return FALSE;
    }
    return TRUE;
}

// ---- file mappings -----------------------------------------------------------------
namespace {
struct Mapping final : Object {
    Mapping() : Object(Kind::Mapping) {}
    int fd = -1;  // -1: anonymous memory
    std::uint64_t size = 0;
    bool writable = false;
};
std::mutex g_views_lock;
std::unordered_map<const void*, std::size_t> g_views;
}  // namespace

HANDLE CreateFileMappingW(HANDLE file, LPSECURITY_ATTRIBUTES, DWORD protect, DWORD size_high, DWORD size_low,
                          LPCWSTR) noexcept {
    auto* mapping = new Mapping();
    mapping->size = (static_cast<std::uint64_t>(size_high) << 32) | size_low;
    mapping->writable = (protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY)) != 0;
    if (File* source = file_of(file)) {
        mapping->fd = source->fd;
        if (mapping->size == 0) {
            struct stat status{};
            if (::fstat(source->fd, &status) == 0) mapping->size = static_cast<std::uint64_t>(status.st_size);
        }
    }
    return mapping;
}
HANDLE CreateFileMappingA(HANDLE file, LPSECURITY_ATTRIBUTES attributes, DWORD protect, DWORD size_high,
                          DWORD size_low, LPCSTR) noexcept {
    return CreateFileMappingW(file, attributes, protect, size_high, size_low, nullptr);
}
LPVOID MapViewOfFile(HANDLE handle, DWORD access, DWORD offset_high, DWORD offset_low, SIZE_T bytes) noexcept {
    if (handle == nullptr || static_cast<Object*>(handle)->kind != Kind::Mapping) return nullptr;
    auto* mapping = static_cast<Mapping*>(handle);
    const std::uint64_t offset = (static_cast<std::uint64_t>(offset_high) << 32) | offset_low;
    if (bytes == 0) bytes = static_cast<SIZE_T>(mapping->size - offset);
    void* view = nullptr;
    if (bytes >= (64u << 20)) {
        std::fprintf(stderr, "[compat] MapViewOfFile %zu MiB (%s)\n", static_cast<std::size_t>(bytes >> 20),
                     mapping->fd < 0 ? "anonymous" : "file");
    }
    if (mapping->fd < 0) {
        // Anonymous memory: the title heap, zeroed
        view = std::calloc(1, bytes);
    } else {
        const bool write = (access & (FILE_MAP_WRITE | FILE_MAP_ALL_ACCESS)) != 0 && mapping->writable;
        view = ::mmap(nullptr, bytes, PROT_READ | (write ? PROT_WRITE : 0), MAP_SHARED, mapping->fd,
                      static_cast<off_t>(offset));
        if (view == MAP_FAILED) view = nullptr;
    }
    if (view == nullptr) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return nullptr;
    }
    std::lock_guard lock(g_views_lock);
    g_views[view] = mapping->fd < 0 ? 0 : bytes;
    return view;
}
BOOL UnmapViewOfFile(LPCVOID view) noexcept {
    std::size_t bytes = 0;
    {
        std::lock_guard lock(g_views_lock);
        const auto it = g_views.find(view);
        if (it == g_views.end()) return FALSE;
        bytes = it->second;
        g_views.erase(it);
    }
    if (bytes == 0) {
        std::free(const_cast<void*>(view));
    } else {
        ::munmap(const_cast<void*>(view), bytes);
    }
    return TRUE;
}
BOOL FlushViewOfFile(LPCVOID, SIZE_T) noexcept { return TRUE; }

// ---- environment ----------------------------------------------------------------------
DWORD GetEnvironmentVariableW(LPCWSTR name, LPWSTR buffer, DWORD size) noexcept {
    const char* value = std::getenv(utf8(name).c_str());
    if (value == nullptr) {
        SetLastError(ERROR_ENVVAR_NOT_FOUND);
        return 0;
    }
    return copy_out(wide(value), buffer, size);
}
DWORD GetEnvironmentVariableA(LPCSTR name, LPSTR buffer, DWORD size) noexcept {
    const char* value = std::getenv(name);
    if (value == nullptr) {
        SetLastError(ERROR_ENVVAR_NOT_FOUND);
        return 0;
    }
    const std::size_t length = std::strlen(value);
    if (buffer == nullptr || size <= length) return static_cast<DWORD>(length + 1);
    std::memcpy(buffer, value, length + 1);
    return static_cast<DWORD>(length);
}
BOOL SetEnvironmentVariableW(LPCWSTR name, LPCWSTR value) noexcept {
    return SetEnvironmentVariableA(utf8(name).c_str(), value != nullptr ? utf8(value).c_str() : nullptr);
}
BOOL SetEnvironmentVariableA(LPCSTR name, LPCSTR value) noexcept {
    return (value != nullptr ? ::setenv(name, value, 1) : ::unsetenv(name)) == 0;
}
DWORD ExpandEnvironmentStringsW(LPCWSTR source, LPWSTR destination, DWORD size) noexcept {
    return copy_out(source != nullptr ? source : L"", destination, size) + 1;
}
LPWSTR GetCommandLineW() noexcept {
    static wchar_t command_line[] = L"eboot.bin";
    return command_line;
}
LPWSTR* CommandLineToArgvW(LPCWSTR, int* count) noexcept {
    static wchar_t program[] = L"eboot.bin";
    static LPWSTR arguments[] = {program, nullptr};
    if (count != nullptr) *count = 1;
    return arguments;
}
LPWCH GetEnvironmentStringsW() noexcept {
    static wchar_t empty[2] = {0, 0};
    return empty;
}
BOOL FreeEnvironmentStringsW(LPWCH) noexcept { return TRUE; }
errno_t getenv_s(std::size_t* required, char* buffer, std::size_t size, const char* name) noexcept {
    const char* value = std::getenv(name);
    if (value == nullptr) {
        if (required != nullptr) *required = 0;
        if (buffer != nullptr && size > 0) buffer[0] = '\0';
        return 0;
    }
    const std::size_t length = std::strlen(value) + 1;
    if (required != nullptr) *required = length;
    if (buffer == nullptr || size < length) return ERANGE;
    std::memcpy(buffer, value, length);
    return 0;
}
errno_t _dupenv_s(char** buffer, std::size_t* count, const char* name) noexcept {
    const char* value = std::getenv(name);
    if (buffer != nullptr) *buffer = value != nullptr ? ::strdup(value) : nullptr;
    if (count != nullptr) *count = value != nullptr ? std::strlen(value) + 1 : 0;
    return 0;
}
errno_t _wdupenv_s(wchar_t** buffer, std::size_t* count, const wchar_t* name) noexcept {
    const char* value = std::getenv(utf8(name).c_str());
    if (buffer != nullptr) *buffer = value != nullptr ? compat::wide_duplicate(wide(value)) : nullptr;
    if (count != nullptr) *count = value != nullptr ? wide(value).size() + 1 : 0;
    return 0;
}
wchar_t* _wgetenv(const wchar_t* name) noexcept {
    // Each answer is kept for the process's life, as the C library's getenv strings are.
    static std::mutex lock;
    static std::unordered_map<std::string, std::wstring> answers;
    const std::string key = utf8(name);
    const char* value = std::getenv(key.c_str());
    if (value == nullptr) return nullptr;
    std::lock_guard guard(lock);
    auto& stored = answers[key];
    stored = wide(value);
    return stored.data();
}
int _putenv_s(const char* name, const char* value) noexcept {
    return (value != nullptr && value[0] != '\0' ? ::setenv(name, value, 1) : ::unsetenv(name)) == 0 ? 0 : EINVAL;
}
int _wputenv_s(const wchar_t* name, const wchar_t* value) noexcept {
    return _putenv_s(utf8(name).c_str(), value != nullptr ? utf8(value).c_str() : nullptr);
}
int _wputenv(const wchar_t* assignment) noexcept {
    const std::string text = utf8(assignment);
    const auto equals = text.find('=');
    if (equals == std::string::npos) return -1;
    return _putenv_s(text.substr(0, equals).c_str(), text.c_str() + equals + 1);
}

// ---- strings ------------------------------------------------------------------------
int MultiByteToWideChar(UINT, DWORD, LPCSTR source, int source_count, LPWSTR destination,
                        int destination_count) noexcept {
    if (source == nullptr) return 0;
    const std::size_t length = source_count < 0 ? std::strlen(source) + 1 : static_cast<std::size_t>(source_count);
    const std::wstring converted = wide(std::string_view(source, length));
    if (destination_count == 0) return static_cast<int>(converted.size());
    if (static_cast<std::size_t>(destination_count) < converted.size()) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return 0;
    }
    std::wmemcpy(destination, converted.data(), converted.size());
    return static_cast<int>(converted.size());
}
int WideCharToMultiByte(UINT, DWORD, LPCWSTR source, int source_count, LPSTR destination, int destination_count,
                        LPCSTR, LPBOOL used_default) noexcept {
    if (source == nullptr) return 0;
    const std::size_t length = source_count < 0 ? std::wcslen(source) + 1 : static_cast<std::size_t>(source_count);
    const std::string converted = utf8(std::wstring_view(source, length));
    if (used_default != nullptr) *used_default = FALSE;
    if (destination_count == 0) return static_cast<int>(converted.size());
    if (static_cast<std::size_t>(destination_count) < converted.size()) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return 0;
    }
    std::memcpy(destination, converted.data(), converted.size());
    return static_cast<int>(converted.size());
}
int lstrlenW(LPCWSTR text) noexcept { return text != nullptr ? static_cast<int>(std::wcslen(text)) : 0; }
int lstrcmpW(LPCWSTR a, LPCWSTR b) noexcept { return std::wcscmp(a, b); }
int lstrcmpiW(LPCWSTR a, LPCWSTR b) noexcept { return _wcsicmp(a, b); }
int _wcsicmp(const wchar_t* a, const wchar_t* b) noexcept {
    for (;; ++a, ++b) {
        const wint_t x = std::towlower(static_cast<wint_t>(*a));
        const wint_t y = std::towlower(static_cast<wint_t>(*b));
        if (x != y || x == 0) return static_cast<int>(x) - static_cast<int>(y);
    }
}
int _wcsnicmp(const wchar_t* a, const wchar_t* b, std::size_t count) noexcept {
    for (; count > 0; --count, ++a, ++b) {
        const wint_t x = std::towlower(static_cast<wint_t>(*a));
        const wint_t y = std::towlower(static_cast<wint_t>(*b));
        if (x != y || x == 0) return static_cast<int>(x) - static_cast<int>(y);
    }
    return 0;
}
int _stricmp(const char* a, const char* b) noexcept { return ::strcasecmp(a, b); }
int _strnicmp(const char* a, const char* b, std::size_t count) noexcept { return ::strncasecmp(a, b, count); }
errno_t wcscpy_s(wchar_t* destination, std::size_t size, const wchar_t* source) noexcept {
    if (destination == nullptr || size == 0) return EINVAL;
    const std::size_t length = std::wcslen(source);
    if (length >= size) {
        destination[0] = L'\0';
        return ERANGE;
    }
    std::wmemcpy(destination, source, length + 1);
    return 0;
}
errno_t wcsncpy_s(wchar_t* destination, std::size_t size, const wchar_t* source, std::size_t count) noexcept {
    if (destination == nullptr || size == 0) return EINVAL;
    std::size_t length = std::wcslen(source);
    if (count != _TRUNCATE && length > count) length = count;
    if (length >= size) {
        if (count != _TRUNCATE) {
            destination[0] = L'\0';
            return ERANGE;
        }
        length = size - 1;
    }
    std::wmemcpy(destination, source, length);
    destination[length] = L'\0';
    return 0;
}
errno_t wcscat_s(wchar_t* destination, std::size_t size, const wchar_t* source) noexcept {
    const std::size_t used = std::wcslen(destination);
    return used < size ? wcscpy_s(destination + used, size - used, source) : ERANGE;
}
errno_t strcpy_s(char* destination, std::size_t size, const char* source) noexcept {
    if (destination == nullptr || size == 0) return EINVAL;
    const std::size_t length = std::strlen(source);
    if (length >= size) {
        destination[0] = '\0';
        return ERANGE;
    }
    std::memcpy(destination, source, length + 1);
    return 0;
}
errno_t strncpy_s(char* destination, std::size_t size, const char* source, std::size_t count) noexcept {
    if (destination == nullptr || size == 0) return EINVAL;
    std::size_t length = std::strlen(source);
    if (count != _TRUNCATE && length > count) length = count;
    if (length >= size) {
        if (count != _TRUNCATE) {
            destination[0] = '\0';
            return ERANGE;
        }
        length = size - 1;
    }
    std::memcpy(destination, source, length);
    destination[length] = '\0';
    return 0;
}
errno_t strcat_s(char* destination, std::size_t size, const char* source) noexcept {
    const std::size_t used = std::strlen(destination);
    return used < size ? strcpy_s(destination + used, size - used, source) : ERANGE;
}
errno_t memcpy_s(void* destination, std::size_t size, const void* source, std::size_t count) noexcept {
    if (count > size) return ERANGE;
    std::memcpy(destination, source, count);
    return 0;
}
errno_t memmove_s(void* destination, std::size_t size, const void* source, std::size_t count) noexcept {
    if (count > size) return ERANGE;
    std::memmove(destination, source, count);
    return 0;
}
std::size_t strnlen_s(const char* text, std::size_t size) noexcept { return text != nullptr ? ::strnlen(text, size) : 0; }
std::size_t wcsnlen_s(const wchar_t* text, std::size_t size) noexcept {
    std::size_t length = 0;
    while (text != nullptr && length < size && text[length] != L'\0') ++length;
    return length;
}

// ---- the C runtime ------------------------------------------------------------------
static std::string mode_utf8(const wchar_t* mode) {
    std::string out = utf8(mode);
    // ",ccs=UTF-8" and the like are MSVC's only
    if (const auto comma = out.find(','); comma != std::string::npos) out.resize(comma);
    return out;
}
errno_t fopen_s(FILE** file, const char* name, const char* mode) noexcept {
    if (file == nullptr) return EINVAL;
    *file = std::fopen(host_path(name).c_str(), mode);
    return *file != nullptr ? 0 : errno;
}
errno_t _wfopen_s(FILE** file, const wchar_t* name, const wchar_t* mode) noexcept {
    if (file == nullptr) return EINVAL;
    *file = std::fopen(host_path(name).c_str(), mode_utf8(mode).c_str());
    return *file != nullptr ? 0 : errno;
}
errno_t _wfopen_s(FILE** file, const char* name, const wchar_t* mode) noexcept {
    if (file == nullptr) return EINVAL;
    *file = std::fopen(host_path(name).c_str(), mode_utf8(mode).c_str());
    return *file != nullptr ? 0 : errno;
}
FILE* _wfopen(const wchar_t* name, const wchar_t* mode) noexcept {
    return std::fopen(host_path(name).c_str(), mode_utf8(mode).c_str());
}
FILE* _wfopen(const char* name, const wchar_t* mode) noexcept {
    return std::fopen(host_path(name).c_str(), mode_utf8(mode).c_str());
}
FILE* _wfsopen(const wchar_t* name, const wchar_t* mode, int) noexcept { return _wfopen(name, mode); }
FILE* _wfsopen(const char* name, const wchar_t* mode, int) noexcept { return _wfopen(name, mode); }
FILE* _fsopen(const char* name, const char* mode, int) noexcept { return std::fopen(host_path(name).c_str(), mode); }
errno_t localtime_s(struct tm* result, const time_t* time) noexcept {
    return ::localtime_r(time, result) != nullptr ? 0 : EINVAL;
}
errno_t gmtime_s(struct tm* result, const time_t* time) noexcept { return ::gmtime_r(time, result) != nullptr ? 0 : EINVAL; }
int _fileno(FILE* file) noexcept { return ::fileno(file); }
int _isatty(int) noexcept { return 0; }
int _setmode(int, int) noexcept { return _O_BINARY; }
_locale_t _create_locale(int, const char*) noexcept {
    static int c_locale;
    return reinterpret_cast<_locale_t>(&c_locale);
}
void _free_locale(_locale_t) noexcept {}
float _strtof_l(const char* text, char** end, _locale_t) noexcept { return std::strtof(text, end); }
double _strtod_l(const char* text, char** end, _locale_t) noexcept { return std::strtod(text, end); }
int _snprintf_s_l(char* buffer, std::size_t size, std::size_t, const char* format, _locale_t locale, ...) noexcept {
    va_list arguments;
    va_start(arguments, locale);
    const int result = std::vsnprintf(buffer, size, format, arguments);
    va_end(arguments);
    return result;
}
int _sprintf_l(char* buffer, const char* format, _locale_t locale, ...) noexcept {
    va_list arguments;
    va_start(arguments, locale);
    const int result = std::vsprintf(buffer, format, arguments);
    va_end(arguments);
    return result;
}
int _vsnprintf_s(char* buffer, std::size_t size, std::size_t, const char* format, va_list arguments) noexcept {
    const int result = std::vsnprintf(buffer, size, format, arguments);
    return result >= 0 && static_cast<std::size_t>(result) < size ? result : -1;
}
int _vsnwprintf_s(wchar_t* buffer, std::size_t size, std::size_t, const wchar_t* format, va_list arguments) noexcept {
    return std::vswprintf(buffer, size, format, arguments);
}
