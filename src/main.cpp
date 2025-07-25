// To execute, Run: cl /EHsc /std:c++17 src\\sha256.cpp src\\base64.cpp src\\main.cpp /Fe:password_manager.exe
// then run password_manager.exe

#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <array>
#include <tuple>
#include <algorithm>
#include <random>
#include <cstdio>

#ifdef _WIN32
#include <conio.h>
#define NOMINMAX
#include <windows.h>
#endif

#include "sha256.h"
#include "base64.h"

using namespace std;
using byte32 = array<uint8_t, 32>;

#ifdef _WIN32
void open_terminal()
{
    FreeConsole();
    if (AllocConsole())
    {
        freopen("CONIN$", "r", stdin);
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        SetConsoleTitleA("Password Manager");
        cout << "#################################################################################################" << endl;
        cout << "#                                                                                               #" << endl;
        cout << "#                                    PASSWORD MANAGER                                           #" << endl;
        cout << "#                                                                                               #" << endl;
        cout << "#################################################################################################" << endl;
    }
}
#endif

static mt19937_64 rng{random_device{}()};

string generate_password(
    size_t length,
    bool use_lower = true,
    bool use_upper = true,
    bool use_digits = true,
    bool use_symbols = false)
{
    static const string lower = "abcdefghijklmnopqrstuvwxyz";
    static const string upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static const string digits = "0123456789";
    static const string symbols = "!@#$%^&*()-_=+[]{}|;:,.<>/?";

    string alphabet;
    if (use_lower)
        alphabet += lower;
    if (use_upper)
        alphabet += upper;
    if (use_digits)
        alphabet += digits;
    if (use_symbols)
        alphabet += symbols;
    if (alphabet.empty())
        throw runtime_error("No character sets selected");

    uniform_int_distribution<size_t> dist(0, alphabet.size() - 1);
    string pwd;
    pwd.reserve(length);
    while (pwd.size() < length)
        pwd += alphabet[dist(rng)];
    return pwd;
}

string get_hidden_input()
{
    string input;
#ifdef _WIN32
    char ch;
    // _getch() is used to take hiddent input in windows(_getch is a part of conio.h)
    while ((ch = _getch()) != '\r')
    {
        // For backspace
        if (ch == '\b' && !input.empty())
        {
            input.pop_back();
            cout << "\b \b";
        }
        // If valid character is entered then do this
        else if (ch >= 32 && ch <= 126)
        {
            input.push_back(ch);
            cout << '*';
        }
    }
    cout << '\n';
#else
// Can use termios for linux for getting hidden input
    cin>>input;
#endif
    return input;
}

byte32 sha256_digest(const string &in)
{
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, (uint8_t *)in.data(), in.size());
    byte32 out;
    sha256_final(&ctx, out.data());
    return out;
}

string xor_crypt(const string &data, const byte32 &key)
{
    string out = data;
    for (size_t i = 0; i < data.size(); ++i)
        out[i] = data[i] ^ key[i % key.size()];
    return out;
}

bool file_exists(const string &path)
{
    ifstream f(path);
    return f.good();
}

int main()
{
#ifdef _WIN32
    open_terminal();
#endif

    const string masterHash = "master.hash";
    const string passVault = "vault.dat";

    cout << "Enter master password: ";
    string master = get_hidden_input();
    byte32 key = sha256_digest(master);

    if (file_exists(masterHash))
    {
        ifstream hin(masterHash);
        string stored_b64;
        getline(hin, stored_b64);
        string stored_raw = base64_decode(stored_b64);
        if (stored_raw.size() != key.size() ||
            !equal(stored_raw.begin(), stored_raw.end(),
                   reinterpret_cast<const char *>(key.data())))
        {
            cerr << "ERROR: Wrong master password.\n";
            return 1;
        }
    }
    else
    {
        cout << "No master set; create one.\nConfirm: ";
        string confirm = get_hidden_input();
        if (confirm != master)
        {
            cerr << "ERROR: passwords did not match.\n";
            return 1;
        }
        ofstream hout(masterHash, ios::trunc);
        hout << base64_encode(
            string(reinterpret_cast<char *>(key.data()), key.size()));
        cout << "Master password saved.\n";
    }

    vector<tuple<string, string>> entries;
    if (file_exists(passVault))
    {
        ifstream fin(passVault);
        string b64;
        fin >> b64;
        string blob = xor_crypt(base64_decode(b64), key);
        istringstream iss(blob);
        string line;
        while (getline(iss, line))
        {
            auto p1 = line.find(':'), p2 = line.find_last_of(':');
            if (p1 != string::npos && p2 > p1)
                entries.emplace_back(
                    line.substr(0, p1),
                    line.substr(p1 + 1, p2 - p1 - 1) + ':' + line.substr(p2 + 1));
        }
    }

    while (true)
    {
        cout << "\n[A]dd [G]et [D]el [L]ist [Q]uit: ";
        char cmd;
        cin >> cmd;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        if (cmd == 'Q' || cmd == 'q')
            break;
        if (cmd == 'A' || cmd == 'a')
        {
            string svc, user;
            cout << "Service: ";
            getline(cin, svc);
            cout << "User:    ";
            getline(cin, user);

            cout << "Generate a random password? (y/N) ";
            string line;
            getline(cin, line);
            char choice = line.empty() ? 'n' : line[0];

            string pass;
            if (choice == 'y' || choice == 'Y')
            {
                cout << "  Length? ";
                size_t len;
                cin >> len;
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                pass = generate_password(len);
                cout << "  Generated: " << pass << "\n";
            }
            else
            {
                cout << "Pass:    ";
                pass = get_hidden_input();
            }
            entries.emplace_back(svc, user + ':' + pass);
            cout << "Entry added.\n";
        }
        else if (cmd == 'G' || cmd == 'g')
        {
            cout << "Service: ";
            string svc;
            getline(cin, svc);
            bool found = false;
            for (auto &t : entries)
            {
                if (get<0>(t) == svc)
                {
                    auto up = get<1>(t);
                    auto pos = up.find(':');
                    cout << "User: " << up.substr(0, pos)
                         << ", Pass: " << up.substr(pos + 1) << '\n';
                    found = true;
                    break;
                }
            }
            if (!found)
                cout << "<not found>\n";
        }
        else if (cmd == 'D' || cmd == 'd')
        {
            cout << "Service: ";
            string svc;
            getline(cin, svc);
            entries.erase(
                remove_if(entries.begin(), entries.end(),
                          [&](auto &t)
                          { return get<0>(t) == svc; }),
                entries.end());
        }
        else if (cmd == 'L' || cmd == 'l')
        {
            for (auto &t : entries)
                cout << " - " << get<0>(t) << "\n";
        }
    }

    ostringstream oss;
    for (auto &t : entries)
        oss << get<0>(t) << ':' << get<1>(t) << '\n';
    ofstream fout(passVault, ios::trunc);
    fout << base64_encode(xor_crypt(oss.str(), key));

    cout << "Vault saved. Goodbye!\n";
    return 0;
}
