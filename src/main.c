#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <Shellapi.h>
#include <locale.h>
#include <ctype.h>
#include <Winuser.h>
#include <curl/curl.h>

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
void StartStream(char **urls, int size, const char *qual, char *search, BOOL mainStream, int limit);
BOOL IsProcessRunning(const char *processName);
// BOOL IsWindowVisible(HWND hwnd);
void GetWindowTitle(HWND hwnd, char *buffer, int bufferSize);
void GetProcessName(DWORD processId, char *buffer, int bufferSize);
// void GetWindowRect(HWND hwnd, RECT* rect);
void streamlink(int n, const char *search, const char *qual, const char *url);
BOOL RunAsAdmin(const char *applicationPath);
char *extractUrl(char *str);
size_t writeCallback(char* data, size_t size, size_t nmemb, char* buffer);

// Global Variables
LASTINPUTINFO lastInputInfo;
char windowTitles[MAX_WINDOWS][MAX_TITLE_LENGTH];
char processNames[MAX_WINDOWS][MAX_PROCESS_NAME_LENGTH];
RECT windowRects[MAX_WINDOWS];
int windowCount = 0;
int runningCount = 0;
BOOL hasArgs = FALSE;
char *cd;


size_t writeCallback(char* data, size_t size, size_t nmemb, char* buffer) {
    size_t total_size = size * nmemb;
    strncat(buffer, data, total_size);
    return total_size;
}
int isStreamerLive(const char *client_id, const char *client_secret, const char *streamer_name)
{
    int is_live = 0;

    // Step 1: Obtain access token
    CURL *curl;
    CURLcode res;
    char post_fields[1024];
    snprintf(post_fields, sizeof(post_fields), "client_id=%s&client_secret=%s&grant_type=client_credentials", client_id, client_secret);

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, "https://id.twitch.tv/oauth2/token");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields);
        // Create a buffer to store the response data
        char response_buffer[10240];
        response_buffer[0] = '\0';

        // Set the CURLOPT_WRITEFUNCTION option to write the response data to a buffer
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);

        // Set the response_buffer as the CURLOPT_WRITEDATA option to store the response data
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, response_buffer);

        // Set the response_buffer as the CURLOPT_PRIVATE option to associate it with the easy handle
        curl_easy_setopt(curl, CURLOPT_PRIVATE, response_buffer);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }
        else
        {
            // Step 2: Retrieve access token from response
            char access_token[1024];
            char* response_data = NULL;
            long response_code;

            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            if (response_code == 200) {
                curl_easy_getinfo(curl, CURLINFO_PRIVATE, &response_data);
                if (response_data != NULL) {
                    const char* token_start = strstr(response_data, "\"access_token\":\"");
                    if (token_start != NULL) {
                        const char* token_end = strstr(token_start + 16, "\"");
                        if (token_end != NULL) {
                            int token_length = token_end - (token_start + 16);
                            strncpy(access_token, token_start + 16, token_length);
                            access_token[token_length] = '\0';
                            printf("Access Token: %s\n", access_token);
                        }
                    }
                }
            }

            // Step 3: Make request to Twitch API
            printf("response: %s\ndata: %s\ntoken: %s\n", response_buffer, response_data, access_token);
            if (strlen(access_token) > 0)
            {
                char url[1024];
                snprintf(url, sizeof(url), "https://api.twitch.tv/helix/streams?user_login=%s", streamer_name);

                struct curl_slist *headers = NULL;
                headers = curl_slist_append(headers, client_id);
                char authorization_header[1024];
                snprintf(authorization_header, sizeof(authorization_header), "Authorization: Bearer %s", access_token);
                headers = curl_slist_append(headers, authorization_header);

                curl_easy_setopt(curl, CURLOPT_URL, url);
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

                res = curl_easy_perform(curl);
                if (res == CURLE_OK)
                {
                    // Step 4: Parse the response and check if the streamer is live
                    char *stream_data;
                    size_t stream_data_size;
                    curl_easy_getinfo(curl, CURLINFO_PRIVATE, &stream_data);
                    curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD_T, &stream_data_size);
                    printf("%s: %s\n", url, stream_data);
                    if (stream_data_size > 0)
                    {
                        // Parse the JSON response and check if the streamer is live
                        // Implement your JSON parsing logic here
                        // Assuming the stream_data is a JSON string
                        // You need to implement your own JSON parsing logic

                        // Example JSON parsing logic:
                        if (strstr(stream_data, "\"type\":\"live\"") != NULL)
                        {
                            is_live = 1;
                        }
                    }
                }
                else
                {
                    fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
                }

                curl_slist_free_all(headers);
            }
        }

        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
    // getchar();
    return is_live;
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
int readLines(FILE *file, char **urls, int limit, int inverse)
{
    char line[256];
    int size = 0;

    // Determine the number of lines in the file
    int lineCount = 0;
    int yt = 0;
    while (fgets(line, sizeof(line), file))
    {
        lineCount++;
        if (strstr(line, "/") != NULL)
        {
            yt = 1;
        }
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
    if (inverse >= 1 && yt != 1)
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
            if (strstr(line, "/") == NULL)
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
int main(int argc, char *argv[])
{
    PSID administrators_group = NULL;
    SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
    BOOL result = AllocateAndInitializeSid(&nt_authority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &administrators_group);
    BOOL is_user_admin = FALSE;
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
    char *qual = "720p,720p60,480p,best";
    char *url = "";
    int limit = 3;
    char *search = "";
    char *path = argv[0];
    BOOL mainStream = FALSE;
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
                    url = argv[4];
                    search = argv[5];
                }
            }
        }
    }
    else
    {
        limit += 200;
    }
    char *lastSeparator = strrchr(path, '\\');
    FILE *file;
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
        char *filePath = malloc(pathLength + strlen("streams.txt") + 1);
        cd = malloc(pathLength + 1);
        strcpy(filePath, programDirectory);
        strcpy(cd, filePath);
        strcat(filePath, "streams.txt");

        // Print the file path
        printf("File path: %s\nCurrent Dir: %s\n", filePath, cd);
        file = fopen(filePath, "r"); // Open the file in read mode
    }
    char **urls = NULL; // Dynamic array to store URLs
    int size = 0;       // Current size of the dynamic array

    if (file)
    {
        // File exists, read its contents and add to the array
        char line[1024];
        int lim = 80;            // Maximum number of URLs to read
        int inverse = limit > 4; // Variable indicating whether to read in inverse order

        urls = malloc(lim * sizeof(char *));
        if (urls == NULL)
        {
            printf("Failed to allocate memory.\n");
            fclose(file);
            return 1;
        }

        size = readLines(file, urls, lim, inverse);
        if (size == -1)
        {
            printf("Failed to read URLs from the file.\n");
            fclose(file);
            for (int i = 0; i < lim; i++)
            {
                free(urls[i]);
            }
            free(urls);
            return 1;
        }
        fclose(file); // Close the file
    }
    else
    {
        // File doesn't exist, initialize an empty array
        urls = malloc(sizeof(char *));
        // urls[0] = strdup(""); // Initialize the first element with an empty string
        size = 0;
    }
    printf("Args: %d Qual: %s\nSearch: %s Main: %d Limit: %d, Size: %d\n", hasArgs, qual, search, mainStream, limit, size);
    StartStream(urls, size, qual, search, mainStream, limit);

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

// Starts the stream
void StartStream(char **urls, int size, const char *qual, char *search, BOOL mainStream, int limit)
{
    windowCount = 0;
    runningCount = 0;
    EnumWindows(EnumWindowsCallback, 0);
    char *url;
    int plyWin = -1;
    // Count the number of running instances of the player
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
            runningCount++;
            for (int j = 0; j < size; j++)
            {
                url = urls[j];
                search = extractVideoID(url);
                *search = tolower(*search);
                printf("Search: %s", search);
                int match = strstr(wn, search) != NULL;
                if (match == 1)
                {
                    printf("\nUrl:%s  -  Search: %s  -  %d\n", url, search, match);
                    free(urls[j]); // Free the memory allocated for the URL
                    // Shift the remaining elements to fill the gap
                    memmove(&urls[j], &urls[j + 1], sizeof(char *) * (size - j - 1));
                    size -= 1;
                    break;
                    // Quit();
                }
            }
            if (windowRects[i].left >= 0 || plyWin == -1)
            {
                plyWin = i;
            }
        }
    }
    printf("Windows: %d, Count: %d/%d, SizeL %d\n", windowCount, runningCount, limit, size);
    // If the running count exceeds the limit, exit the program
    DWORD idle;
    char config_file[1296];
    strcat(config_file, cd);
    strcat(config_file, "config.txt");
    printf("Config: %s\n", config_file);
    // const char *config_file = "config.txt";
    char client_id[MAX_LENGTH];
    char client_secret[MAX_LENGTH];

    // Read client ID, client secret, and streamer name from the config file
    FILE *file = fopen(config_file, "r");
    if (file == NULL)
    {
        fprintf(stderr, "Failed to open config file: %s\n", config_file);
    }
    else
    {
        // Read client ID
        if (fgets(client_id, sizeof(client_id), file) == NULL)
        {
            fprintf(stderr, "Failed to read client ID from config file\n");
            fclose(file);
        }
        client_id[strcspn(client_id, "\n")] = '\0'; // Remove trailing newline

        // Read client secret
        if (fgets(client_secret, sizeof(client_secret), file) == NULL)
        {
            fprintf(stderr, "Failed to read client secret from config file\n");
            fclose(file);
        }
        client_secret[strcspn(client_secret, "\n")] = '\0'; // Remove trailing newline
        fclose(file);
    }
    int auth = strlen(client_id) > 4 && strlen(client_secret) > 4;
    printf("ID: %s\nSecret: %s\nAuth: %d\n", client_id, client_secret, auth);

    char *streamer_name;
    int is_live = 1;
    // Start the stream based on the window configuration
    // Print the URLs in the array
    for (int i = 0; i < size; i++)
    {
        if (runningCount >= limit)
        {
            // break;
            //  Quit();
        }
        url = urls[i];
        printf("URL %d: %s\n", i + 1, url);
        if (strstr(url, "twitch.tv") != NULL && auth)
        {
            streamer_name = extractVideoID(url);
            printf("\n%s::%d\n", streamer_name, is_live);
            is_live = isStreamerLive(client_id, client_secret, streamer_name);
        }
        if (is_live)
        {
            printf("Streamer is live!\n");
            idle = GetIdleDuration();
            int i = plyWin;
            printf("Rectangle %d: left=%d, top=%d, right=%d, bottom=%d\n",
                   i, windowRects[i].left, windowRects[i].top,
                   windowRects[i].right, windowRects[i].bottom);
            char *tit = WindowActiveTitle();
            int yt = (strstr(tit, "YouTube") != NULL);
            printf("Opening, Main: %d, Idle: %d\nActiveTitle: %s, Yt: %d\n", mainStream, idle, tit, yt);
            int ply = plyWin == -1 ? 0 : windowRects[plyWin].left < 0;
            printf("Ply: %d\nWin: %d\n", ply, windowRects[plyWin].left);
            if ((idle > 150 && ply && !mainStream && !yt) || mainStream)
            {
                windowRects[plyWin].left = 1;
                streamlink(1, search, qual, url);
            }
            else
            {
                streamlink(0, search, qual, url);
            }
            runningCount++;
        }
    }

    // Free the memory allocated for URLs
    for (int i = 0; i < size; i++)
    {
        free(urls[i]);
    }
    free(urls);
}

void streamlink(int n, const char *search, const char *qual, const char *url)
{

    char command[1024];
    sprintf(command, "streamlink %s \"%s\" --player-args=\"--fullscreen=yes --volume=0 --fs-screen=%d --cache=yes --window-maximized=yes\"", url, qual, n);
    printf("!!! %d %s %s %s\n", n, search, qual, url);
    printf("%s\n", command);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);

    if (CreateProcessA(NULL, command, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        // Wait for the process to finish if needed
        // WaitForSingleObject(pi.hProcess, INFINITE);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}