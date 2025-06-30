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
#include <limits>
#include <random>
#include <stdexcept>
#include <cstdio> // for freopen

#ifdef _WIN32
#include <conio.h>   // _getch() for hidden input(using *)
#define NOMINMAX     // prevent windows.h from defining min/max macros
#include <windows.h> // Console API
#endif

#include "sha256.h"
#include "base64.h"

using byte32 = std::array<uint8_t, 32>;

#ifdef _WIN32
// Opens a new console window and redirects std streams
void open_new_console()
{
    FreeConsole();
    if (AllocConsole())
    {
        freopen("CONIN$", "r", stdin);
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        SetConsoleTitleA("🔐 Password Manager");
        // Print heading banner
        std::cout << "#################################################################################################" << std::endl;
        std::cout << "#                                                                                               #" << std::endl;
        std::cout << "#                                    PASSWORD MANAGER                                           #" << std::endl;
        std::cout << "#                                                                                               #" << std::endl;
        std::cout << "#################################################################################################" << std::endl;

    }
}
#endif

// One-time RNG seeded once
static std::mt19937_64 rng{std::random_device{}()};

std::string generate_password(
    size_t length,
    bool use_lower = true,
    bool use_upper = true,
    bool use_digits = true,
    bool use_symbols = false)
{
    static const std::string lower = "abcdefghijklmnopqrstuvwxyz";
    static const std::string upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static const std::string digits = "0123456789";
    static const std::string symbols = "!@#$%^&*()-_=+[]{}|;:,.<>/?";

    std::string alphabet;
    if (use_lower)
        alphabet += lower;
    if (use_upper)
        alphabet += upper;
    if (use_digits)
        alphabet += digits;
    if (use_symbols)
        alphabet += symbols;
    if (alphabet.empty())
        throw std::runtime_error("No character sets selected");

    std::uniform_int_distribution<size_t> dist(0, alphabet.size() - 1);
    std::string pwd;
    pwd.reserve(length);
    while (pwd.size() < length)
    {
        pwd += alphabet[dist(rng)];
    }
    return pwd;
}

std::string get_hidden_input()
{
    std::string input;
#ifdef _WIN32
    char ch;
    while ((ch = _getch()) != '\r')
    {
        if (ch == '\b' && !input.empty())
        {
            input.pop_back();
            std::cout << "\b \b";
        }
        else if (ch >= 32 && ch <= 126)
        {
            input.push_back(ch);
            std::cout << '*';
        }
    }
    std::cout << '\n';
#else
    termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    std::getline(std::cin, input);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
#endif
    return input;
}

byte32 sha256_digest(const std::string &in)
{
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx,
                  reinterpret_cast<const uint8_t *>(in.data()),
                  in.size());
    byte32 out;
    sha256_final(&ctx, out.data());
    return out;
}

std::string xor_crypt(const std::string &data, const byte32 &key)
{
    std::string out = data;
    for (size_t i = 0; i < data.size(); ++i)
        out[i] = data[i] ^ key[i % key.size()];
    return out;
}

bool file_exists(const std::string &path)
{
    std::ifstream f(path);
    return f.good();
}

int main()
{
#ifdef _WIN32
    open_new_console();
#endif

    const std::string HASH_FILE = "master.hash";
    const std::string VAULT_FILE = "vault.dat";

    std::cout << "Enter master password: ";
    std::string master = get_hidden_input();
    byte32 key = sha256_digest(master);

    if (file_exists(HASH_FILE))
    {
        std::ifstream hin(HASH_FILE);
        std::string stored_b64;
        std::getline(hin, stored_b64);
        std::string stored_raw = base64_decode(stored_b64);
        if (stored_raw.size() != key.size() ||
            !std::equal(stored_raw.begin(), stored_raw.end(),
                        reinterpret_cast<const char *>(key.data())))
        {
            std::cerr << "ERROR: Wrong master password.\n";
            return 1;
        }
    }
    else
    {
        std::cout << "No master set; create one.\nConfirm: ";
        std::string confirm = get_hidden_input();
        if (confirm != master)
        {
            std::cerr << "ERROR: passwords did not match.\n";
            return 1;
        }
        std::ofstream hout(HASH_FILE, std::ios::trunc);
        hout << base64_encode(
            std::string(reinterpret_cast<char *>(key.data()), key.size()));
        std::cout << "Master password saved.\n";
    }

    std::vector<std::tuple<std::string, std::string>> entries;
    if (file_exists(VAULT_FILE))
    {
        std::ifstream fin(VAULT_FILE);
        std::string b64;
        fin >> b64;
        std::string blob = xor_crypt(base64_decode(b64), key);
        std::istringstream iss(blob);
        std::string line;
        while (std::getline(iss, line))
        {
            auto p1 = line.find(':'), p2 = line.find_last_of(':');
            if (p1 != std::string::npos && p2 > p1)
            {
                entries.emplace_back(
                    line.substr(0, p1),
                    line.substr(p1 + 1, p2 - p1 - 1) + ':' + line.substr(p2 + 1));
            }
        }
    }

    while (true)
    {
        std::cout << "\n[A]dd [G]et [D]el [L]ist [Q]uit: ";
        char cmd;
        std::cin >> cmd;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        if (cmd == 'Q' || cmd == 'q')
            break;
        if (cmd == 'A' || cmd == 'a')
        {
            std::string svc, user;
            std::cout << "Service: ";
            std::getline(std::cin, svc);
            std::cout << "User:    ";
            std::getline(std::cin, user);

            std::cout << "Generate a random password? (y/N) ";
            std::string line;
            std::getline(std::cin, line);
            char choice = line.empty() ? 'n' : line[0];

            std::string pass;
            if (choice == 'y' || choice == 'Y')
            {
                std::cout << "  Length? ";
                size_t len;
                std::cin >> len;
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                pass = generate_password(len);
                std::cout << "  Generated: " << pass << "\n";
            }
            else
            {
                std::cout << "Pass:    ";
                pass = get_hidden_input();
            }
            entries.emplace_back(svc, user + ':' + pass);
            std::cout << "Entry added.\n";
        }
        else if (cmd == 'G' || cmd == 'g')
        {
            std::cout << "Service: ";
            std::string svc;
            std::getline(std::cin, svc);
            bool found = false;
            for (auto &t : entries)
            {
                if (std::get<0>(t) == svc)
                {
                    auto up = std::get<1>(t);
                    auto pos = up.find(':');
                    std::cout << "User: " << up.substr(0, pos)
                              << ", Pass: " << up.substr(pos + 1) << '\n';
                    found = true;
                    break;
                }
            }
            if (!found)
                std::cout << "<not found>\n";
        }
        else if (cmd == 'D' || cmd == 'd')
        {
            std::cout << "Service: ";
            std::string svc;
            std::getline(std::cin, svc);
            entries.erase(
                std::remove_if(entries.begin(), entries.end(),
                               [&](auto &t)
                               { return std::get<0>(t) == svc; }),
                entries.end());
        }
        else if (cmd == 'L' || cmd == 'l')
        {
            for (auto &t : entries)
                std::cout << " - " << std::get<0>(t) << "\n";
        }
    }

    std::ostringstream oss;
    for (auto &t : entries)
        oss << std::get<0>(t) << ':' << std::get<1>(t) << '\n';
    std::ofstream fout(VAULT_FILE, std::ios::trunc);
    fout << base64_encode(xor_crypt(oss.str(), key));

    std::cout << "Vault saved. Goodbye!\n";
    return 0;
}
