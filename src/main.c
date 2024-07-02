#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <Shellapi.h>
#include <locale.h>
#include <ctype.h>
#include <time.h>
#include <Winuser.h>
#include <curl/curl.h>
#include <math.h>

#pragma comment(lib, "User32.lib")

#define MAX_LENGTH 1024
#define MAX_TITLE_LENGTH 1024
#define MAX_PROCESS_NAME_LENGTH 1024
#define MAX_WINDOWS 1000

typedef struct
{
    UINT cbSize;
    DWORD dwTime;
} LASTINPT;

// Function Declarations
BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam);
void GetLastInput(LASTINPUTINFO *lastInputInfo);
DWORD GetIdleDuration();
void Quit();
BOOL IsProcessRunning(const char *processName);
// BOOL IsWindowVisible(HWND hwnd);
void GetWindowTitle(HWND hwnd, char *buffer, int bufferSize);
void GetProcessName(DWORD processId, char *buffer, int bufferSize);
// void GetWindowRect(HWND hwnd, RECT* rect);
void streamlink(int n, const char *search, const char *qual, const char *url, HANDLE *processes, int *numProcesses);
BOOL RunAsAdmin(const char *applicationPath);
char *extractUrl(char *str);
void StartStream(char **urls, int size, char **filePaths, const char *qual, char *search, BOOL mainStream, int limit, char **titles, int titleSize, char **watched, char **watchedTitles, HANDLE *processes);
int run_node_process(const char *args);

// Global Variables
LASTINPUTINFO lastInputInfo;
char windowTitles[MAX_WINDOWS][MAX_TITLE_LENGTH];
char processNames[MAX_WINDOWS][MAX_PROCESS_NAME_LENGTH];
RECT windowRects[MAX_WINDOWS];
int windowCount = 0;
int runningCount = 0;
int started =  1;
int wait = 1;
BOOL hasArgs = FALSE;
char *cd;
char *qual = "720p,720p60,480p,best";
char *vt = "1";
int limit = 3;
char *search = "";
char *stream = "both";
BOOL mainStream = FALSE;
int yt = 0;
int both = 0;
int numProcesses = 0;

// Callback function for writing data in the response body
typedef struct
{
    char *buffer;
    int isLiveBroadcastFound;
} WriteCallbackData;

size_t writeCallback(char *data, size_t size, size_t nmemb, void *userdata)
{
    size_t total_size = size * nmemb;
    WriteCallbackData *callbackData = (WriteCallbackData *)userdata;
    strncat(callbackData->buffer, data, total_size);

    return total_size;
}

int isStreamerLive(const char *channelName)
{
    CURL *curl;
    CURLcode res;
    int isLive = 0;
    char url[100];
    char responseBody[1248000] = ""; // Buffer to store response body

    // Build the URL
    snprintf(url, sizeof(url), "https://www.twitch.tv/%s", channelName);

    // Initialize curl
    curl = curl_easy_init();
    if (curl)
    {
        // Set the URL
        curl_easy_setopt(curl, CURLOPT_URL, url);

        // Set the write callback function
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);

        // Create callback data
        WriteCallbackData callbackData;
        callbackData.buffer = responseBody;
        callbackData.isLiveBroadcastFound = 0;

        // Set the userdata for the callback
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &callbackData);

        // Perform the request
        res = curl_easy_perform(curl);

        // Check if the request was successful and 'isLiveBroadcast' was found
        int is = strstr(callbackData.buffer, "isLiveBroadcast") != NULL;
        // fprintf(file, callbackData.buffer);
        if (res == CURLE_OK && is)
        {
            isLive = 1;
        }

        // Cleanup curl
        curl_easy_cleanup(curl);
    }

    return isLive;
}

BOOL RunAsAdmin(const char *applicationPath)
{
    SHELLEXECUTEINFO sei = {sizeof(SHELLEXECUTEINFO)};
    LPCSTR verb = "runas"; // Narrow string literal
    sei.lpVerb = verb;

    sei.lpFile = (LPCSTR)applicationPath;
    sei.hwnd = NULL;
    sei.nShow = SW_NORMAL;

    if (!ShellExecuteEx(&sei))
    {
        DWORD errorCode = GetLastError();
        if (errorCode == ERROR_CANCELLED)
        {
            // User declined the elevation prompt
            return FALSE;
        }
        else
        {
            // Failed to launch the process with admin privileges
            return FALSE;
        }
    }

    // The process has been launched with admin privileges
    return TRUE;
}
char *CurrentDir()
{
    static char path[MAX_PATH];
    DWORD length = GetModuleFileNameA(NULL, path, MAX_PATH);

    if (length == 0)
    {
        printf("Failed to retrieve the file path.\n");
        return NULL;
    }

    return path;
}
char *extractUrl(char *str)
{
    // printf("Str: %s\n", str);
    char *lastSlash = strrchr(str, '/');

    if (lastSlash != NULL)
    {
        // printf("Last slash found at position %ld\n", lastSlash - str);
        // printf("Extracted substring: %s\n", ++lastSlash);
        return lastSlash + 1;
    }
    else
    {
        printf("No slash found in the string.\n");
    }
}
// Function to extract the video ID from a YouTube URL
char *extractVideoID(char *url)
{
    const char *startMarker = "watch?v=";
    const char *endMarker = "&";

    // Find the start of the video ID
    const char *start = strstr(url, startMarker);
    if (start == NULL)
    {
        // printf("Invalid YouTube URL\n");
        return extractUrl(url); // NULL;
    }

    // Move the pointer to the beginning of the video ID
    start += strlen(startMarker);

    // Find the end of the video ID
    const char *end = strstr(start, endMarker);
    if (end == NULL)
    {
        // If the end marker is not found, assume the video ID extends to the end of the URL
        end = url + strlen(url);
    }

    // Calculate the length of the video ID
    size_t length = end - start;

    // Allocate memory for the video ID string
    char *videoID = (char *)malloc((length + 1) * sizeof(char));
    if (videoID == NULL)
    {
        printf("Memory allocation failed\n");
        return NULL;
    }

    // Copy the video ID to the allocated memory
    strncpy(videoID, start, length);
    videoID[length] = '\0';

    return videoID;
}
// Function to remove newline character from a string
void removeNewline(char *str)
{
    if (str[strlen(str) - 1] == '\n')
    {
        str[strlen(str) - 1] = '\0';
    }
}

// Function to prepend "https://www.twitch.tv/" to a string
char *prependTwitchURL(const char *str)
{
    char *result = malloc(strlen(str) + strlen("https://www.twitch.tv/") + 1);
    if (result != NULL)
    {
        strcpy(result, "https://www.twitch.tv/");
        strcat(result, str);
    }
    return result;
}

// Function to read file lines in a specified order
int readLines(FILE *file, char **urls, int limit, int inverse, int title)
{
    char line[256];
    int size = 0;

    // Determine the number of lines in the file
    int lineCount = 0;
    while (fgets(line, sizeof(line), file))
    {
        lineCount++;
    }

    // Reset the file pointer to the beginning of the file
    fseek(file, 0, SEEK_SET);

    // Adjust the limit if it is greater than the number of lines
    if (limit > lineCount)
    {
        limit = lineCount;
    }
    printf("Inverse: %d, Yt: %d, Lines: %d\n", inverse, yt, limit);
    // Read the file lines based on the inverse variable
    if (inverse >= 1)
    {
        // Read in inverse order
        char **tempUrls = malloc(limit * sizeof(char *));
        if (tempUrls == NULL)
        {
            return -1; // Memory allocation failed
        }

        int i = lineCount - 1;

        while (fgets(line, sizeof(line), file) && size < limit)
        {
            if (i < lineCount - limit)
            {
                i--;
                continue; // Skip lines beyond the limit
            }

            removeNewline(line);
            if (strstr(line, "/") == NULL || title == 0)
            {
                tempUrls[size] = prependTwitchURL(line);
            }
            else
            {
                tempUrls[size] = malloc(strlen(line) + 1); // Allocate memory for the string
                if (tempUrls[size] != NULL)
                {
                    strcpy(tempUrls[size], line);
                }
                else
                {
                    // Handle memory allocation failure
                    perror("malloc");
                }
            }
            if (tempUrls[size] == NULL)
            {
                // Memory allocation failed, free previously allocated URLs
                for (int j = 0; j < size; j++)
                {
                    free(tempUrls[j]);
                }
                free(tempUrls);
                return -1;
            }

            size++;
            i--;
        }

        // Copy the URLs from the temporary array to the main array in the correct order
        for (int j = 0; j < size; j++)
        {
            urls[j] = tempUrls[size - 1 - j];
        }

        free(tempUrls); // Free the memory allocated for the temporary array
    }
    else
    {
        // Read in normal order
        while (fgets(line, sizeof(line), file) && size < limit)
        {
            removeNewline(line);
            if (strstr(line, "/") == NULL)
            {
                urls[size] = prependTwitchURL(line);
            }
            else
            {
                urls[size] = malloc(strlen(line) + 1); // Allocate memory for the string
                if (urls[size] != NULL)
                {
                    strcpy(urls[size], line);
                }
                else
                {
                    // Handle memory allocation failure
                    perror("malloc");
                }
            }
            if (urls[size] == NULL)
            {
                // Memory allocation failed, free previously allocated URLs
                for (int j = 0; j < size; j++)
                {
                    free(urls[j]);
                }
                return -1;
            }

            size++;
        }
    }
    for (int i = 0; i < size; i++)
    {
        printf("URL %d/%d: %s\n", i + 1, size, urls[i]);
    }

    return size;
}

char penult(char *str)
{

    int len = strlen(str);
    int lastBackslashIndex = -1;
    int secondLastBackslashIndex = -1;

    // Find the last and second-to-last backslash indices
    for (int i = 0; i < len; i++)
    {
        if (str[i] == '\\')
        {
            if (lastBackslashIndex == -1)
            {
                lastBackslashIndex = i;
            }
            else
            {
                secondLastBackslashIndex = lastBackslashIndex;
                lastBackslashIndex = i;
            }
        }
    }

    // Check if there are at least 2 backslashes
    if (secondLastBackslashIndex == -1)
    {
        return '\0'; // Return null character if there are less than 2 backslashes
    }

    return str[secondLastBackslashIndex];
}
char ***makeArray(int array_x, int array_y, int array_z)
{
    char ***a3d = (char ***)malloc(array_x * sizeof(char **));
    for (int i = 0; i < array_x; i++)
    {
        a3d[i] = (char **)malloc(array_y * sizeof(char *));
        for (int j = 0; j < array_y; j++)
        {
            a3d[i][j] = (char *)malloc(array_z * sizeof(char));
            // Initialize the memory block to null characters
            memset(a3d[i][j], '\0', array_z * sizeof(char));
        }
    }
    return a3d;
}
void process(char **filePaths, char **watched, char **watchedTitles, HANDLE *processes)
{
    if (strstr(stream, "yt") != NULL || both)
    {
        yt = 1;
        char node[128] = "node ../js/live.js 0 50 ";
        strcat(node, vt);
        strcat(node, " > node.txt");
        printf("%s\n", node);
        int result = 1; //system(node);
        if (result == -1)
        {
            // An error occurred while spawning the process
            printf("Failed to spawn Node.js process\n");
        }
    }
    int *sizes = malloc(3 * sizeof(int)); // Array to store the current sizes of the dynamic arrays
    char ***urls = makeArray(3, 256, 512);
    FILE *files[2];
    files[0] = fopen(filePaths[0], "r"); // Open the file in read mode
    if (both)
    {
        files[1] = fopen(filePaths[1], "r"); // Open the file in read mode
    }

    if (files[0] || files[1])
    {
        // Files exist, read their contents and add to the arrays
        int inverse = limit > 4 && strstr(stream, "yt") == NULL; // Variable indicating whether to read in inverse order
        for (int i = 0; i < (both ? 2 : 1); i++)
        {
            if (i == 1 || (both == 0 && yt == 1))
            {
                inverse = 0;
            }
            sizes[i] = readLines(files[i], urls[i], 256, inverse, 0);
            if (sizes[i] == -1)
            {
                printf("Failed to read URLs from file %d.\n", i);
                fclose(files[i]);
                for (int j = 0; j < sizes[i]; j++)
                {
                    free(urls[i][j]);
                }
                free(urls[i]);
                return;
            }
            fclose(files[i]); // Close the file
        }
    }
    FILE *titleFile = fopen(filePaths[2], "r");
    char **titles = malloc(256 * sizeof(char *));
    int titleSize = readLines(titleFile, titles, 256, 0, 1);
    fclose(titleFile);
    for (int i = 0; i < titleSize; i++)
    {
        printf("Title %d: %s\n", i, titles[i]);
    }
    // Use the urls, files, and sizes arrays as needed
    char *streamer_name;
    int is_live;
    int count = 0;
    for (int i = 0; i < sizes[0]; i++)
    {
        printf("URL %d/%d: %s\n", i + 1, sizes[0], urls[0][i]);
        if (strstr(urls[0][i], "twitch.tv") != NULL)
        {
            streamer_name = extractVideoID(urls[0][i]);
            is_live = isStreamerLive(streamer_name);
            printf("\n%s::%d\n", streamer_name, is_live);
            if (is_live)
            {
                // Keep the live streamer in the array
                urls[0][count] = urls[0][i];
                count++;
            }
        }
    }
    // Resize the urls[0] array to the number of live streamers
    if (count > 0)
    {
        urls[0] = (char **)realloc(urls[0], count * sizeof(char *));
    }
    int score = pow(2, count);

    for (int i = 0; i < count; i++)
    {
        printf(": URL-1 %d: %s\n", i, urls[0][i]);
    }
    if (both)
    {
        int c = 0;
        sizes[2] = 0;
        for (int i = 0; i < sizes[1]; i++)
        {
            printf("URL-2 %d/%d: %s\n", i + 1, sizes[1], urls[1][i]);
        }
        /*
f(0) = 4096 * (0.25)^0 = 4096
f(1) = 4096 * (0.25)^1 = 1024
f(2) = 4096 * (0.25)^2 = 256
f(3) = 4096 * (0.25)^3 = 64
f(4) = 4096 * (0.25)^4 = 16
*/
        for (int y = 0; y < sizes[1]; y++)
        {
            int point = 4096 * pow(0.25, sizes[2]);
            if (point == 4096)
            {
                point = 0;
            }
            printf("%d:   %d - %d  %d   %d - %d\n", sizes[2], y, c, count, point, score);
            if ((point >= score) && c < count)
            {
                strcpy(urls[2][sizes[2]], urls[0][c]);
                c++;
                sizes[2]++;
                y--;
            }
            else
            {
                strcpy(urls[2][sizes[2]], urls[1][y]);
                sizes[2]++;
            }
        }
        while (c < count)
        {
            strcpy(urls[2][sizes[2]], urls[0][c]);
            c++;
            sizes[2]++;
        }
        for (int i = 0; i < sizes[2]; i++)
        {
            printf("URL-3 %d: %s\n", i, urls[2][i]);
        }
    }

    // Clean up memory
    /*for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < sizes[i]; j++)
        {
            free(urls[i][j]);
        }
        free(urls[i]);
    }*/
    // free(sizes);
    for (int i = 0; i < 3; i++)
    {
        printf("%d\n", sizes[i]);
    }
    printf("Args: %d Qual: %s\nSearch: %s Main: %d Limit: %d, Size: %d\n", hasArgs, qual, search, mainStream, limit, count);
    if (both)
    {
        StartStream(urls[2], sizes[2], filePaths, qual, search, mainStream, limit, titles, titleSize, watched, watchedTitles, processes);
    }
    else
    {
        if (strstr(stream, "yt") != NULL)
        {
            StartStream(urls[0], sizes[0], filePaths, qual, search, mainStream, limit, titles, titleSize, watched, watchedTitles, processes);
        }
        else
        {
            StartStream(urls[0], count, filePaths, qual, search, mainStream, limit, titles, titleSize, watched, watchedTitles, processes);
        }
    }
}
int run_node_process(const char *args)
{
#ifdef _WIN32
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (CreateProcess(NULL, (char *)args, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return (int)exitCode;
    }
    else
    {
        return -1;
    }
#else
    pid_t pid = fork();
    if (pid == 0)
    {
        // Child process
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);

        int result = system(args);
        exit(result);
    }
    else if (pid > 0)
    {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status))
        {
            return WEXITSTATUS(status);
        }
        else
        {
            return -1;
        }
    }
    else
    {
        return -1;
    }
#endif
}

int main(int argc, char *argv[])
{
    PSID administrators_group = NULL;
    SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
    BOOL result = AllocateAndInitializeSid(&nt_authority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &administrators_group);
    BOOL is_user_admin = FALSE;
    char *path = argv[0];
    char *currentPath = CurrentDir();

    if (result)
    {
        CheckTokenMembership(NULL, administrators_group, &is_user_admin);
        FreeSid(administrators_group);
    }

    if (is_user_admin)
    {
        printf("Admin\n");
    }
    else
    {
        // Get the current executable path
        printf("Not Admin, path: %s\n", currentPath);
        // Run the current executable as admin
        if (!RunAsAdmin(currentPath))
        {
            // Handle the failure to elevate privileges
            return 1;
        }
        // The elevated process has started successfully
        return 0;
    }

    // Set the output stream encoding to UTF-8
    setlocale(LC_ALL, "en_US.UTF-8");
    printf("CD: %s\n", path);

    if (argc > 1)
    {
        hasArgs = TRUE;
        qual = argv[1];
        if (argc > 2)
        {
            limit = atoi(argv[2]);
            if (argc > 3)
            {
                mainStream = atoi(argv[3]);
                if (argc > 4)
                {
                    vt = argv[4];
                    wait = atoi(argv[5]);
                    if (argc > 6)
                    {
                        stream = argv[6];
                    }
                }
            }
        }
    }
    else
    {
        //limit += 7;
        //stream = "yt.txt";
        // vt = 2;
    }
    printf("Stream: %s, limit: %d, Vt: %s, Qual: %s\n", stream, limit, vt, qual);
    // limit = 3;
    char *lastSeparator = strrchr(path, '\\');
    char *filePaths[3];
    printf("%s : %d\n", path, lastSeparator != NULL);
    if (lastSeparator != NULL)
    {
        // Calculate the length of the path
        size_t pathLength = lastSeparator - path + 1;

        // Allocate memory for the path string
        char *programDirectory = malloc(pathLength + 1);

        // Copy the program's directory to the new string
        strncpy(programDirectory, path, pathLength);
        programDirectory[pathLength] = '\0';

        // Concatenate the file name to the directory path
        filePaths[0] = malloc(pathLength + strlen(stream) + 10);
        filePaths[1] = malloc(pathLength + strlen(stream) + 10);
        filePaths[2] = malloc(pathLength + strlen(stream) + 10);
        char *cd = malloc(pathLength + 1);
        strcpy(filePaths[0], programDirectory);
        strcpy(cd, filePaths[0]);
        strcpy(filePaths[2], filePaths[0]);
        strcat(filePaths[2], "titles.txt");
        if (strstr(stream, "both") != NULL)
        {
            strcpy(filePaths[1], filePaths[0]);
            strcat(filePaths[0], "streams.txt");
            strcat(filePaths[1], "yt.txt");
            both = 1;
        }
        else
        {
            strcat(filePaths[0], stream);
        }
        // Print the file path
        printf("File paths:\n");
        for (int i = 0; i < 3; i++)
        {
            printf("%s\n", filePaths[i]);
        }
        printf("Current Dir: %s\n", cd);
        // Initialize the pointers to the matrix
    }
    char **watched = malloc(4096 * sizeof(char *));
    char **watchedTitles = malloc(4096 * sizeof(char *));
    
    // Initialize the watched array elements
    for (int i = 0; i < 4096; i++)
    {
        watched[i] = malloc(sizeof(char) * 512);
        if (watched[i] == NULL)
        {
            // Handle memory allocation failure
            // Free previously allocated memory and return
            for (int j = 0; j < i; j++)
            {
                free(watched[j]);
            }
            free(watched);
            return 0;
        }
        strcpy(watched[i], ""); // Initialize the string to an empty string
    }
    for (int i = 0; i < 4096; i++)
    {
        watchedTitles[i] = malloc(sizeof(char) * 512);
        if (watchedTitles[i] == NULL)
        {
            // Handle memory allocation failure
            // Free previously allocated memory and return
            for (int j = 0; j < i; j++)
            {
                free(watchedTitles[j]);
            }
            free(watchedTitles);
            return 0;
        }
        strcpy(watchedTitles[i], ""); // Initialize the string to an empty string
    }
    HANDLE *processes;
    // Initialize the processes array

    // Initialize the processes array
    processes = (HANDLE *)malloc(sizeof(HANDLE) * limit);

    process(filePaths, watched, watchedTitles, processes);

    if (!hasArgs)
    {
        // getchar();
    }
    return 0;
}
void GetWindowTitle(HWND hwnd, char *buffer, int bufferSize)
{
    if (GetWindowTextA(hwnd, buffer, bufferSize) == 0)
    {
        buffer[0] = '\0';
    }
}

void GetProcessName(DWORD processId, char *buffer, int bufferSize)
{
    HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (processHandle != NULL)
    {
        HMODULE moduleHandles[1];
        DWORD cbNeeded;
        if (EnumProcessModules(processHandle, moduleHandles, sizeof(moduleHandles), &cbNeeded))
        {
            if (GetModuleBaseNameA(processHandle, moduleHandles[0], buffer, bufferSize) == 0)
            {
                buffer[0] = '\0';
            }
        }
        CloseHandle(processHandle);
    }
}
// Callback function for EnumWindows
BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam)
{
    if (IsWindowVisible(hwnd))
    {
        char windowTitle[256];
        char processName[256];
        GetWindowTitle(hwnd, windowTitle, sizeof(windowTitle));
        DWORD processId;
        GetWindowThreadProcessId(hwnd, &processId);
        GetProcessName(processId, processName, sizeof(processName));
        strncpy(windowTitles[windowCount], windowTitle, sizeof(windowTitles[windowCount]));
        strncpy(processNames[windowCount], processName, sizeof(processNames[windowCount]));
        GetWindowRect(hwnd, &windowRects[windowCount]);
        windowCount++;
    }
    return TRUE;
}

DWORD GetLastInputTime()
{
    LASTINPUTINFO lastInputInfo;
    lastInputInfo.cbSize = sizeof(LASTINPUTINFO);
    GetLastInputInfo(&lastInputInfo);
    return lastInputInfo.dwTime;
}
// Calculates the idle duration in seconds
DWORD GetIdleDuration()
{
    DWORD last = GetLastInputTime();

    DWORD currentTickCount = GetTickCount();
    DWORD idleDuration = currentTickCount - last;
    idleDuration /= 1000;
    printf("\n    - LastInput %d / %d\n", idleDuration, last);

    return idleDuration;
}

// Exits the program
void Quit()
{
    if (!hasArgs)
    {
        // getchar();
    }
    exit(0);
}
char *WindowActiveTitle()
{
    HWND foregroundWindow = GetForegroundWindow();
    if (foregroundWindow != NULL)
    {
        int bufferSize = GetWindowTextLength(foregroundWindow) + 1;
        char *windowTitle = malloc(bufferSize * sizeof(char));
        if (windowTitle != NULL)
        {
            GetWindowTextA(foregroundWindow, windowTitle, bufferSize);
            return windowTitle;
        }
        else
        {
            printf("Memory allocation failed.\n");
        }
    }
    else
    {
        printf("No active window found.\n");
    }

    return NULL;
}
int *getCurrentTime()
{
    // Create an array to hold the hour and minute values
    static int timeArray[2];

    // Get the current time
    time_t currentTime = time(NULL);

    // Convert the current time to local time
    struct tm *localTime = localtime(&currentTime);

    // Extract the hour and minute from the local time
    timeArray[0] = localTime->tm_hour;
    timeArray[1] = localTime->tm_min;

    // Return a pointer to the timeArray
    return timeArray;
}
void wins(char **urls, int *size, int limit, char **titles, int titleSize, int plyWin){
    char *url;
    windowCount = 0;
    EnumWindows(EnumWindowsCallback, 0);
    for (int i = 0; i < windowCount; i++)
    {
        printf("Index %d: left=%d, top=%d, right=%d, bottom=%d\n",
               i, windowRects[i].left, windowRects[i].top,
               windowRects[i].right, windowRects[i].bottom);
        char *wn = windowTitles[i];
        *wn = tolower(*wn);
        printf("- %s -- %s %d\n", processNames[i], windowTitles[i]);
        if (strstr(processNames[i], "mpv.exe") != NULL)
        {
            if(started)
                runningCount++;
            int match = 0;
            for (int j = 0; j < *size; j++)
            {
                url = urls[j];
                search = extractVideoID(url);
                //*search = tolower(*search);
                printf("Search: %s\n", search);
                match = strstr(wn, search) != NULL;
                if (match == 1)
                {
                    printf("\nUrl:%s  -  Search: %s  -  %d\n", url, search, match);
                    free(urls[j]); // Free the memory allocated for the URL
                    // Shift the remaining elements to fill the gap
                    memmove(&urls[j], &urls[j + 1], sizeof(char *) * (*size - j - 1));
                    (*size)--;
                    break;
                    // Quit();
                }
            }
            if (match == 0 && yt)
            {
                for (int k = 0; k < titleSize; k++)
                {
                    //*titles[k] = (char)tolower(*titles[k]);
                    printf("Tit: %s\n", titles[k]);
                    if (strstr(wn, titles[k]) != NULL)
                    {
                        free(urls[k]); // Free the memory allocated for the URL
                        // Shift the remaining elements to fill the gap
                        memmove(&urls[k], &urls[k + 1], sizeof(char *) * (*size - k - 1));
                        (*size)--;
                        break;
                    }
                }
            }
            if (windowRects[i].left >= 0 || plyWin == -1)
            {
                plyWin = i;
            }
        }
    }
}
// Starts the stream
void StartStream(char **urls, int size, char **filePaths, const char *qual, char *search, BOOL mainStream, int limit, char **titles, int titleSize, char **watched, char **watchedTitles, HANDLE *processes)
{
    char *url;
    static int watches = 0;
    // Count the number of running instances of the player
    int plyWin = -1;
    wins(urls, &size, limit, titles, titleSize, plyWin);
    printf("Windows: %d, Count: %d/%d, Size: %d\n", windowCount, runningCount, limit, size);
    // If the running count exceeds the limit, exit the program
    DWORD idle;

    // printf("ID: %s\nSecret: %s\nAuth: %d\n", client_id, client_secret, auth);

    char *streamer_name;
    int is_live = 1;
        
    char *exceptions[] = {"YouTube", "Twitch", "Netflix", "Migaku", "Animelon"};
    int numExceptions = sizeof(exceptions) / sizeof(exceptions[0]);

    int yt = 0;

    printf("Processes: %d\n", numProcesses);
    for (int i = 0; i < numProcesses; i++)
    {
        printf("Process %d:\n", i);
        printf("  Process ID: %lu\n", GetProcessId((HANDLE)processes[i]));
    }
    int n;
    // Start the stream based on the window configuration
    // Print the URLs in the array
    int matches = 0;
    started = 0;
    for (int i = 0; i < size; i++)
    {
        // break;
        //   Quit();
        url = urls[i];
        int can = 1;
        printf(titles[i]);
        strcpy(watchedTitles[watches], titles[i]);
        for (int i = 0; i < watches; i++)
        {
            printf("- %s\n", watched[i]);
            if (strstr(url, watched[i]) != NULL)
            {
                can = 0;
                matches += 1;
            }
        }
        if (matches == size && wait == 0)
        {
            return;
        }
        if (can)
        {
            printf("URL %d: %s\n", i + 1, url);
            idle = GetIdleDuration();
            int i = plyWin;
            printf("Rectangle %d: left=%d, top=%d, right=%d, bottom=%d\n",
                   i, windowRects[i].left, windowRects[i].top,
                   windowRects[i].right, windowRects[i].bottom);
            int *times = getCurrentTime();
            char *tit = WindowActiveTitle();
            for (int i = 0; i < numExceptions; i++)
            {
                if (strstr(tit, exceptions[i]) != NULL)
                {
                    yt = 1;
                    // break;
                }
            }
            printf("Opening, Main: %d, Idle: %d\nActiveTitle: %s, Yt: %d\n", mainStream, idle, tit, yt);
            int ply;
            if (plyWin != -2)
            {
                ply = plyWin == -1 ? 1 : windowRects[plyWin].left < 0;
                if (plyWin == -1)
                {
                    plyWin = -2;
                }
            }
            else
            {
                ply = 0;
            }
            int cond1 = (idle > 590 && ply && !mainStream);
            int cond2 = (ply && !yt && !mainStream);
            int cond = cond1 || cond2 || mainStream;
            printf("Ply: %d = %d\nWin: %d\n", ply, cond, windowRects[plyWin].left);
            if (cond)
            {
                windowRects[plyWin].left = 1;
                streamlink(1, search, qual, url, processes, &numProcesses);
            }
            else
            {
                streamlink(0, search, qual, url, processes, &numProcesses);
            }
            strcpy(watched[watches], url);
            watches++;
            runningCount++;
        }
        if (runningCount >= limit || i == size - 1)
        {
            if (!wait)
            {
                break;
            }
            DWORD waitResult = WaitForMultipleObjects(numProcesses, processes, FALSE, INFINITE);
            if (waitResult >= WAIT_OBJECT_0 && waitResult < WAIT_OBJECT_0 + numProcesses)
            {
                int terminatedIndex = waitResult - WAIT_OBJECT_0;
                CloseHandle(processes[terminatedIndex]);
                for (int i = terminatedIndex; i < numProcesses - 1; i++)
                {
                    processes[i] = processes[i + 1];
                }
                numProcesses--;
                process(filePaths, watched, watchedTitles, processes);
                return;
            }
        }
        wins(urls, &size, limit, titles, titleSize, plyWin);
    }

    // Free the memory allocated for URLs
    for (int i = 0; i < size; i++)
    {
        free(urls[i]);
    }
    free(urls);
}

void streamlink(int n, const char *search, const char *qual, const char *url, HANDLE *processes, int *numProcesses)
{
    char command[1024];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    // Prepare the command line
    if(strstr(url, "twitch") != NULL){
        sprintf(command, "streamlink %s \"%s\" --player-args=\"--fullscreen=yes --volume=0 --fs-screen=%d --cache=yes --window-maximized=yes\"", url, qual, n);
    } else {
        sprintf(command, "mpv %s --fullscreen=yes --volume=0 --fs-screen=%d --cache=yes --window-maximized=yes", url, n);
    }
    printf("!!! %d %s %s %s\n", n, search, qual, url);
    printf("%s\n", command);

    // Create the new process
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);

    if (CreateProcessA(NULL, command, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        // Add the process information to the array
        processes[*numProcesses] = pi.hProcess;
        (*numProcesses)++;
    }
}