#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include "curlComms.hpp"
#include "RSJparser.tcc"

#include <iostream>

#include <string>
#include <chrono>
#include <thread>
#include <fstream>
#include <vector>
#include <algorithm>

#include <Windows.h>

using namespace std;

curlComms cc;

bool isError(string s)
{
    transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return std::tolower(c); }); // Make it lowercase

    string tok = "";

    for (int i = 0; i < s.length(); i++)
    {
        if (s[i] == 'e')
            tok = "e";
        else
            tok = tok + s[i];

        if (tok == "error")
            return true;
    }

    return false;
}

int main()
{
    results t;
    
    cc.changeHeader("Content-Type: application/x-www-form-urlencoded"); // Normal for auth setup.
    t = cc.setupConnection("password", "<api_url>", "<client_id>", "<client_secret>", "<user_email>", "<user_password>");
    cc.changeHeader("Content-Type: application/json  ");

    if (t.success == true)
    {        
            string ret = cc.getUser("<user_id>");
            RSJresource myjson(ret); // Lets process the feedback.
            
            //cout<<"Name = "<< myjson["name"]["displayName"].as<std::string>() << "\n";
            //cout << "------------------------------------------------------------------\n";
            //cout << "Return = " << ret << "\n";
            
            bool er = isError(ret); // Catch HTML errors

            if (er)
                cout << "Was an error\n";
            else
                cout << "Was OK\n";
        }
    }
    else 
        cout << "Success was false. "<<t.code<<"\n";

    return 0;
}
