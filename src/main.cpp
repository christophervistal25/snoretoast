/*
    SnoreToast is capable to invoke Windows 8 toast notifications.
    Copyright (C) 2013-2019  Hannah von Reth <vonreth@kde.org>

    SnoreToast is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    SnoreToast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with SnoreToast.  If not, see <http://www.gnu.org/licenses/>.
    */
#include "snoretoasts.h"
#include "toasteventhandler.h"
#include "utils.h"

#include "linkhelper.h"

#include <shellapi.h>
#include <roapi.h>

#include <string>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <vector>

using namespace Windows::Foundation;

void help(const std::wstring &error)
{
    if (!error.empty()) {
        std::wcerr << error << std::endl;
    } else {
        std::wcerr << L"Welcome to SnoreToast " << SnoreToasts::version() << "." << std::endl
                   << L"A command line application which is capable of creating Windows Toast "
                      L"notifications."
                   << std::endl;
    }
    std::wcerr
            << std::endl
            << L"---- Usage ----" << std::endl
            << L"SnoreToast [Options]" << std::endl
            << std::endl
            << L"---- Options ----" << std::endl
            << L"[-t] <title string>\t| Displayed on the first line of the toast." << std::endl
            << L"[-m] <message string>\t| Displayed on the remaining lines, wrapped." << std::endl
            << L"[-b] <button1;button2 string>| Displayed on the bottom line, can list multiple "
               L"buttons separated by ;"
            << std::endl
            << L"[-tb]\t\t\t| Displayed a textbox on the bottom line, only if buttons are not "
               L"presented."
            << std::endl
            << L"[-p] <image URI>\t| Display toast with an image, local files only." << std::endl
            << L"[-w] \t\t\t| Wait for toast to expire or activate." << std::endl
            << L"[-id] <id>\t\t| sets the id for a notification to be able to close it later."
            << std::endl
            << L"[-s] <sound URI> \t| Sets the sound of the notifications, for possible values see "
               L"http://msdn.microsoft.com/en-us/library/windows/apps/hh761492.aspx."
            << std::endl
            << L"[-silent] \t\t| Don't play a sound file when showing the notifications."
            << std::endl
            << L"[-appID] <App.ID>\t| Don't create a shortcut but use the provided app id."
            << std::endl
            << L"[-pipeName] <\\.\\pipe\\pipeName\\>\t| Provide a name pipe which is used for "
               L"callbacks."
            << std::endl
            << L"[-application] <C:\\foo.exe>\t| Provide a application that might be started if "
               L"the pipe does not exist."
            << std::endl
            << L"-close <id>\t\t| Closes a currently displayed notification, in order to be able "
               L"to close a notification the parameter -w must be used to create the notification."
            << std::endl
            << std::endl
            << L"-install <name> <application> <appID>| Creates a shortcut <name> in the start "
               L"menu which point to the executable <application>, appID used for the "
               L"notifications."
            << std::endl
            << std::endl
            << L"-v \t\t\t| Print the version and copying information." << std::endl
            << L"-h\t\t\t| Print these instructions. Same as no args." << std::endl
            << L"Exit Status\t:  Exit Code" << std::endl
            << L"Failed\t\t: " << static_cast<int>(SnoreToastActions::Actions::Error) << std::endl
            << std::endl
            << "Success\t\t:  " << static_cast<int>(SnoreToastActions::Actions::Clicked)
            << std::endl
            << "Hidden\t\t:  " << static_cast<int>(SnoreToastActions::Actions::Hidden) << std::endl
            << "Dismissed\t:  " << static_cast<int>(SnoreToastActions::Actions::Dismissed)
            << std::endl
            << "TimedOut\t:  " << static_cast<int>(SnoreToastActions::Actions::Timedout)
            << std::endl
            << "ButtonPressed\t:  " << static_cast<int>(SnoreToastActions::Actions::ButtonClicked)
            << std::endl
            << "TextEntered\t:  " << static_cast<int>(SnoreToastActions::Actions::TextEntered)
            << std::endl
            << std::endl
            << L"---- Image Notes ----" << std::endl
            << L"Images must be .png with:" << std::endl
            << L"\tmaximum dimensions of 1024x1024" << std::endl
            << L"\tsize <= 200kb" << std::endl
            << L"These limitations are due to the Toast notification system." << std::endl;
}

void version()
{
    std::wcerr << L"SnoreToast version " << SnoreToasts::version() << std::endl
               << L"Copyright (C) 2019  Hannah von Reth <vonreth@kde.org>" << std::endl
               << L"SnoreToast is free software: you can redistribute it and/or modify" << std::endl
               << L"it under the terms of the GNU Lesser General Public License as published by"
               << std::endl
               << L"the Free Software Foundation, either version 3 of the License, or" << std::endl
               << L"(at your option) any later version." << std::endl;
}

SnoreToastActions::Actions parse(std::vector<wchar_t *> args)
{
    HRESULT hr = S_OK;

    std::wstring appID;
    std::filesystem::path pipe;
    std::filesystem::path application;
    std::wstring title;
    std::wstring body;
    std::filesystem::path image;
    std::wstring id;
    std::wstring sound(L"Notification.Default");
    std::wstring buttons;
    bool silent = false;
    bool wait = false;
    bool closeNotify = false;
    bool isTextBoxEnabled = false;

    auto nextArg = [&](std::vector<wchar_t *>::const_iterator &it,
                       const std::wstring &helpText) -> std::wstring {
        if (it != args.cend()) {
            return *it++;
        } else {
            help(helpText);
            exit(static_cast<int>(SnoreToastActions::Actions::Error));
        }
    };

    auto it = args.begin() + 1;
    while (it != args.end()) {
        std::wstring arg(nextArg(it, L""));
        std::transform(arg.begin(), arg.end(), arg.begin(),
                       [](int i) -> int { return ::tolower(i); });
        if (arg == L"-m") {
            body = nextArg(it,
                           L"Missing argument to -m.\n"
                           L"Supply argument as -m \"message string\"");
        } else if (arg == L"-t") {
            title = nextArg(it,
                            L"Missing argument to -t.\n"
                            L"Supply argument as -t \"bold title string\"");
        } else if (arg == L"-p") {
            std::wstring path = nextArg(it,
                                        L"Missing argument to -p."
                                        L"Supply argument as -p \"image path\"");
            if (path.substr(0, 8) != L"file:///") {
                image = L"file:///";
                path = _wfullpath(nullptr, path.c_str(), MAX_PATH);
            }
            image.append(path);
        } else if (arg == L"-w") {
            wait = true;
        } else if (arg == L"-s") {
            sound = nextArg(it,
                            L"Missing argument to -s.\n"
                            L"Supply argument as -s \"sound name\"");
        } else if (arg == L"-id") {
            id = nextArg(it,
                         L"Missing argument to -id.\n"
                         L"Supply argument as -id \"id\"");
        } else if (arg == L"-silent") {
            silent = true;
        } else if (arg == L"-appid") {
            appID = nextArg(it,
                            L"Missing argument to -appID.\n"
                            L"Supply argument as -appID \"Your.APP.ID\"");
        } else if (arg == L"-pipename") {
            pipe = nextArg(it,
                           L"Missing argument to -pipeName.\n"
                           L"Supply argument as -pipeName \"\\.\\pipe\\foo\\\"");
        } else if (arg == L"-application") {
            application = nextArg(it,
                                  L"Missing argument to -pipeName.\n"
                                  L"Supply argument as -applicatzion \"C:\\foo.exe\"");
        } else if (arg == L"-b") {
            buttons = nextArg(it,
                              L"Missing argument to -b.\n"
                              L"Supply argument for buttons as -b \"button1;button2\"");
        } else if (arg == L"-tb") {
            isTextBoxEnabled = true;
        } else if (arg == L"-install") {
            const std::wstring shortcut(
                    nextArg(it,
                            L"Missing argument to -install.\n"
                            L"Supply argument as -install \"path to your shortcut\" \"path to the "
                            L"application the shortcut should point to\" \"App.ID\""));

            const std::wstring exe(
                    nextArg(it,
                            L"Missing argument to -install.\n"
                            L"Supply argument as -install \"path to your shortcut\" \"path to the "
                            L"application the shortcut should point to\" \"App.ID\""));

            appID = nextArg(it,
                            L"Missing argument to -install.\n"
                            L"Supply argument as -install \"path to your shortcut\" \"path to the "
                            L"application the shortcut should point to\" \"App.ID\"");

            return SUCCEEDED(LinkHelper::tryCreateShortcut(shortcut, exe, appID))
                    ? SnoreToastActions::Actions::Clicked
                    : SnoreToastActions::Actions::Error;
        } else if (arg == L"-close") {
            id = nextArg(it,
                         L"Missing agument to -close"
                         L"Supply argument as -close \"id\"");
            closeNotify = true;
        } else if (arg == L"-v") {
            version();
            return SnoreToastActions::Actions::Clicked;
        } else if (arg == L"-h") {
            help(L"");
            return SnoreToastActions::Actions::Clicked;
        } else {
            std::wstringstream ws;
            ws << L"Unknown argument: " << arg << std::endl;
            help(ws.str());
            return SnoreToastActions::Actions::Error;
        }
    }
    if (closeNotify) {
        if (!id.empty()) {
            SnoreToasts app(appID);
            app.setId(id);
            if (app.closeNotification()) {
                return SnoreToastActions::Actions::Clicked;
            }
        } else {
            help(L"Close only works if an -id id was provided.");
        }
    } else {
        hr = (title.length() > 0 && body.length() > 0) ? S_OK : E_FAIL;
        if (SUCCEEDED(hr)) {
            if (appID.empty()) {
                std::wstringstream _appID;
                _appID << L"Snore.DesktopToasts." << SnoreToasts::version();
                appID = _appID.str();
                hr = LinkHelper::tryCreateShortcut(appID);
            }
            if (SUCCEEDED(hr)) {
                if (isTextBoxEnabled) {
                    if (pipe.empty()) {
                        std::wcerr << L"TextBox notifications only work if a pipe for the result "
                                      L"was provided"
                                   << std::endl;
                        return SnoreToastActions::Actions::Error;
                    };
                }
                SnoreToasts app(appID);
                app.setPipeName(pipe);
                app.setApplication(application);
                app.setSilent(silent);
                app.setSound(sound);
                app.setId(id);
                app.setButtons(buttons);
                app.setTextBoxEnabled(isTextBoxEnabled);
                app.displayToast(title, body, image, wait);
                return app.userAction();
            }
        } else {
            help(L"");
            return SnoreToastActions::Actions::Clicked;
        }
    }

    return SnoreToastActions::Actions::Error;
}

SnoreToastActions::Actions handleEmbedded()
{
    Utils::registerActivator();
    CToastNotificationActivationCallback::waitForActivation();
    Utils::unregisterActivator();
    return SnoreToastActions::Actions::Clicked;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, wchar_t *, int)
{
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE *dummy;
        _wfreopen_s(&dummy, L"CONOUT$", L"w", stdout);
        setvbuf(stdout, nullptr, _IONBF, 0);

        _wfreopen_s(&dummy, L"CONOUT$", L"w", stderr);
        setvbuf(stderr, nullptr, _IONBF, 0);
        std::ios::sync_with_stdio();
    }
    const auto commandLine = GetCommandLineW();
    tLog << commandLine;
    int argc;
    wchar_t **argv = CommandLineToArgvW(commandLine, &argc);
    SnoreToastActions::Actions action = SnoreToastActions::Actions::Clicked;

    HRESULT hr = Initialize(RO_INIT_MULTITHREADED);
    if (SUCCEEDED(hr)) {
        if (std::wstring(commandLine).find(L"-Embedding") != std::wstring::npos) {
            action = handleEmbedded();
        } else {
            action = parse(std::vector<wchar_t *>(argv, argv + argc));
        }
        Uninitialize();
    }

    return static_cast<int>(action);
}
