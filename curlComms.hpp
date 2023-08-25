// Curl wrapper
// Cameron King 2023

/*
Copyright 2023 Cameron King and libCurl authors

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation 
files (the “Software”), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify,
merge, publish, distribute, sublicense, and/or sell copies of the 
Software, and to permit persons to whom the Software is furnished to 
do so, subject to the following conditions:

The above copyright notice and this permission notice shall be 
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, 
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

// feedback2string still needs exception catching for robustness.

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>

#include <iomanip>
#include <chrono>
#include <ctime>

#include <curl\curl.h>

#define CURL_STATICLIB

using namespace std;

stringstream ss;
string cc_access_token = "";

struct results
{
    bool success = false;
    int code = 0;
};

struct returnpair
{
    string key[16];
    string value[16];
    int count = 0;
};

string timeString()
{
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    string tim;

    stringstream ssx;

    ssx << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    ssx >> tim;

    return tim;
}

size_t feedback2string(void* ptr, size_t size, size_t nmemb, string& stream)
{
    if (ptr == nullptr || size == 0 || nmemb == 0)
    {
        // Handle invalid input data
        cout << "API feedback given invalid pointer!\n";
        return 0; // Indicate an error to Curl
    }

    const char* data = static_cast<const char*>(ptr);
    string temp(data, size * nmemb);
	
    // Ensure proper initialization of 'stream'
    if (stream.empty())
        stream = temp;
    else
    {
        // Reserve space for the new data
	stream.reserve(stream.size() + temp.size());
	stream += temp;
    }

    return size * nmemb;
}

class curlComms
{
public:

    curlComms();
    
    void resetHeader();
    void changeHeader(string dataHeader);
    void setAccessToken(string at, string url);
	results setupConnection(string grantType, string url, string clientId, string clientSecret, string username, string password);

    string restGET(string requestUrl);
    string restPOST(string requestUrl, string body);

    string createUser( string email, string password );

    string getUser(string userId);
    string getCatalog(string key);
    string postComment(string challengeId, string text);
    results addSegmentForUser(string userId);
    string postBooking(string userId, string points, string desc);
    string deleteUser(string userId);

    ~curlComms();

private:

    returnpair tokenizeReturnHeader(string s);
    string fetchToken(returnpair rp, string s);

    CURL* curl;
    CURLcode curl_res;
    string theURL;
    string fullURL;
    struct curl_slist* data_header = curl_slist_append(data_header, "Content-Type: application/json  ");
};

curlComms::curlComms()
{
    curl = curl_easy_init();
}

curlComms::~curlComms()
{

    curl_easy_cleanup(curl);
}

returnpair curlComms::tokenizeReturnHeader(string s)
{
    returnpair rp;

    int mode = 1;

    for (int i = 0; i < s.size(); i++)
    {
        if (mode == 1) // Its the key part
        {
            if (s[i] == ':')
                mode = 2;
            else if (s[i] != '\"' && s[i] != '{' && s[i] != '}')
                rp.key[rp.count].push_back(s[i]);
        }
        else if (mode == 2) // Its the value part
        {
            if (s[i] == ',')
            {
                mode = 1;
                rp.count++;
            }
            else if (s[i] != '\"' && s[i] != '{' && s[i] != '}')
                rp.value[rp.count].push_back(s[i]);
        }
    }
    rp.count++; // End of line won't push the count, so we do.

    return rp;
}

string curlComms::fetchToken(returnpair rp, string s)
{
    string rs = "";

    for (int i = 0; i < rp.count; i++)
    {
        if (rp.key[i] == s)
            rs = rp.value[i];
    }

    return rs;
}

void curlComms::changeHeader(string dataHeader)
{
    curl_slist_free_all(data_header);
    data_header = NULL;
    data_header = curl_slist_append(data_header, dataHeader.c_str());
    cout << "Changing header to '"<< dataHeader<<"'\n";
}

void curlComms::resetHeader()
{
    curl_slist_free_all(data_header);
    data_header = NULL;
    data_header = curl_slist_append(data_header, "Content-Type: application/json");
    cout << "Resetting header to 'Content-Type: application/json  '\n";
}

void curlComms::setAccessToken(string at, string url)
{
    cc_access_token = at;
    theURL = url;
}

results curlComms::setupConnection(string grantType, string url, string clientId, string clientSecret, string username, string password)
{
    results r;

    //struct curl_slist* primary_header = NULL;
    string content;
    string feedback;

    string construct0 = "";
    string construct1 = "";
    string construct2 = "";
    string construct3 = "";
    string construct4 = "";

    if (grantType == "password")
    {
        construct0 = "/oauth/token";
        construct1 = "grant_type=password&username=";
        construct2 = "&password=";
        construct3 = "&scope=all&client_id=";
        construct4 = "&client_secret=";
    }
    else if (grantType == "client")
    {
        construct0 = "/oauth/token";
        construct1 = "grant_type=client_credentials&scope=all"; // YOU NEED SCOPE ALL, I just turned it off for debugging.
        construct2 = "&client_id=";
        construct3 = "&client_secret=";
    }
    else
    {
        r.success = false;
        r.code = -100; // CURL didn't initialize!
        return r;
    }

    if (curl)
    {
        if (grantType == "password")
            content = construct1 + username + construct2 + password + construct3 + clientId + construct4 + clientSecret;
        else if (grantType == "client")
            content = construct1 + construct2 + clientId + construct3 + clientSecret;

        theURL = url;
        fullURL = theURL + construct0;
        cout << "Full URL = " << fullURL << "\n";
        cout << "Content = " << content << "\n";
        curl_easy_setopt(curl, CURLOPT_URL, fullURL.c_str());

       // primary_header = curl_slist_append(primary_header, "Content-Type: application/x-www-form-urlencoded");

        //curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1); // Checks SSL looks good.
        //curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2);

        // Disable SSL certificate verification
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); // Debugging mode

        curl_easy_setopt(curl, CURLOPT_POST, true);

        curl_easy_setopt(curl, CURLOPT_USERAGENT, "curlComms/1.5.0");

       // curl_easy_setopt(curl, CURLOPT_SUPPRESS_CONNECT_HEADERS, 1L); // Experiment to hold back Host and User Agent info until connection established.

        //curl_easy_setopt(curl, CURLOPT_HTTPHEADER, primary_header);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER,data_header);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, content.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(content.c_str()));

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, feedback2string);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &feedback);

        curl_res = curl_easy_perform(curl);

        if (curl_res == CURLE_OK)
        {
            cout << "feedback = " << feedback <<"\n";

            returnpair rp = tokenizeReturnHeader(feedback);
            cc_access_token = fetchToken(rp, "access_token");

            cout << "Access Token = '" << cc_access_token << "'\n";

            if (cc_access_token != "")
            {
                r.success = true;
                r.code = CURLE_OK;
            }
            else
            {
                r.success = false;
                r.code = -1;
            }

            return r;
        }
        else
        {
            // Code 6 means it could not resolve host.
            r.success = false;
            r.code = curl_res;
            return r;
        }
    }
    else
    {
        r.success = false;
        r.code = -100; // CURL didn't initialize!
        return r;
    }
}

string curlComms::restGET(string requestUrl)
{
    string feedback;
    fullURL = theURL + requestUrl;

    curl_easy_setopt(curl, CURLOPT_URL, fullURL.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
    curl_easy_setopt(curl, CURLOPT_XOAUTH2_BEARER, cc_access_token.c_str());

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, feedback2string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &feedback);

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, data_header);
    curl_easy_setopt(curl, CURLOPT_POST, false);

    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

    curl_res = curl_easy_perform(curl);

    if (curl_res == CURLE_OK)
    {
        return feedback;
    }
    else
    {
        cout << "GET failed for some reason.\n";
        return "";
    }
}

string curlComms::restPOST(string requestUrl, string body)
{
    string s;

    return s;
}

string curlComms::getUser(string userId)
{
    string feedback;
    string construct0 = "/2/users/";
    fullURL = theURL + construct0 + userId;

    cout << "Full URL = " << fullURL << "\n";

    curl_easy_setopt(curl, CURLOPT_URL, fullURL.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
    curl_easy_setopt(curl, CURLOPT_XOAUTH2_BEARER, cc_access_token.c_str());

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, feedback2string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &feedback);

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, data_header);
    curl_easy_setopt(curl, CURLOPT_POST, false);

    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

    curl_res = curl_easy_perform(curl);

    if (curl_res == CURLE_OK) 
    {
        return feedback;
    }
    else
    {
        cout << "user GET failed for some reason.\n";
        return "";
    }
}

string curlComms::getCatalog(string key)
{
    string feedback;
    string construct0 = "/2/catalog/messages?keys=";
    fullURL = theURL + construct0 + key;

    cout << "Full URL = " << fullURL << "\n";

    curl_easy_setopt(curl, CURLOPT_URL, fullURL.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
    curl_easy_setopt(curl, CURLOPT_XOAUTH2_BEARER, cc_access_token.c_str());

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, feedback2string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &feedback);

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, data_header);
    curl_easy_setopt(curl, CURLOPT_POST, false);

    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

    curl_res = curl_easy_perform(curl);

    if (curl_res == CURLE_OK)
    {
        return feedback;
    }
    else
    {
        cout << "user GET failed for some reason.\n";
        return "";
    }
}

string curlComms::postComment(string challengeId, string text)
{
    string feedback;
    string construct0 = "/2/challenges/";
    string construct1 = "/comments";
    fullURL = theURL + construct0 + challengeId + construct1;

    //cout << "URL = " << fullURL << "\n";

    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    string tim;

    stringstream ss;

    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    ss >> tim;

    string testJson = R"zzz(
{   
"text" : ")zzz" + text + "\",\n" +
    R"zzz("time" : ")zzz" + tim + "\"\n}";

    //cout << "json = " << testJson << "\n";;

    results r;

    curl_easy_setopt(curl, CURLOPT_URL, fullURL.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
    curl_easy_setopt(curl, CURLOPT_XOAUTH2_BEARER, cc_access_token.c_str());

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, testJson.c_str());

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, feedback2string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &feedback);

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, data_header);

    curl_res = curl_easy_perform(curl);

    if (curl_res == CURLE_OK)
    {
        return feedback;
    }
    else
    {
        cout << "postComment failed for some reason.\n";
        return "";
    }
}

string curlComms::createUser(string email, string password)
{
    string feedback;
    string construct0 = "/2/users";

    fullURL = theURL + construct0;

    string testJson = R"zzz(
    {
    "relation": "moderated",
    "public": false,
    "email": "<user_email>",
    "password":"<password>"
    }
    )zzz";

    results r;

    curl_easy_setopt(curl, CURLOPT_URL, fullURL.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
    curl_easy_setopt(curl, CURLOPT_XOAUTH2_BEARER, cc_access_token.c_str());

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, testJson.c_str());

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, feedback2string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &feedback);

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, data_header);

    curl_res = curl_easy_perform(curl);

    if (curl_res == CURLE_OK)
    {
        return feedback;
    }
    else
    {
        cout << "createUser failed for some reason.\n";
        return "";
    }
}

string curlComms::postBooking(string userId, string points, string desc)
{
    string feedback;
    string construct0 = "/2/users/";
    string construct1 = "/bookings";
    string fullURL = theURL + construct0 + userId + construct1;

    cout << "URL = " << fullURL << "\n";

    string testJson = R"zzz(
    {
        "time" : ")zzz" + timeString() + R"zzz(",
        "text" : ")zzz" + desc + R"zzz(",
        "earned" : )zzz" + points + R"zzz(,
        "currency" : "<currency_type>",
        "kind" : "booking"
    })zzz";
 
    cout << "json = " << testJson << "\n";

    curl_easy_setopt(curl, CURLOPT_URL, fullURL.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
    curl_easy_setopt(curl, CURLOPT_XOAUTH2_BEARER, cc_access_token.c_str());

    //headers = curl_slist_append(headers, "Content-Type: text/plain");

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, testJson.c_str());
    //curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(testJson.c_str())); // DEBUGGING, THIS NEEDS TO BE ENABLED.

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, feedback2string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &feedback);

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, data_header);

    curl_res = curl_easy_perform(curl);

    if (curl_res == CURLE_OK)
    {
        return feedback;
    }
    else
    {
        return "-1";
    }
}

string curlComms::deleteUser(string userId)
{
    // /2/users?emails={{userEmail}}

    string feedback;
    string construct0 = "/2/users/";
    string fullURL = theURL + construct0 + userId;

    cout << "URL = " << fullURL << "\n";

    string testJson = R"zzz(
    {
    })zzz";

    //cout << "json = " << testJson << "\n";

    curl_easy_setopt(curl, CURLOPT_URL, fullURL.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
    curl_easy_setopt(curl, CURLOPT_XOAUTH2_BEARER, cc_access_token.c_str());

    //headers = curl_slist_append(headers, "Content-Type: text/plain");

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, data_header);

    // Set the request method to DELETE
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");

    curl_res = curl_easy_perform(curl);

    if (curl_res == CURLE_OK)
    {
        return feedback;
    }
    else
    {
        return "-1";
    }
}
