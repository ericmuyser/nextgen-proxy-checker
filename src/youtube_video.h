#ifndef PROXOS_YOUTUBE_VIDEO
#define PROXOS_YOUTUBE_VIDEO

#include "nextgen/common.h"
#include "nextgen/network.h"
#include "nextgen/database.h"
#include "nextgen/social.h"

#include "proxy_checker.h"

bool YOUTUBE_DEBUG_1 = 1;

namespace youtube
{
    struct video_variables
    {
        video_variables() : id(nextgen::null_str), session_views(0)
        {

        }

        std::string id;
        uint32_t session_views;
    };

    class video
    {
        NEXTGEN_ATTACH_SHARED_VARIABLES(video, video_variables);
    };

    struct account_variables : public nextgen::social::basic_account_variables
    {
        typedef nextgen::social::basic_account_variables base_type;

        account_variables() : base_type()
        {

        }
    };

    class account : public nextgen::social::basic_account<account_variables>
    {
        public: struct types
        {
            static const uint32_t none = 0;
            static const uint32_t google = 1;
            static const uint32_t youtube = 2;
        };

        NEXTGEN_ATTACH_SHARED_BASE(account, nextgen::social::basic_account<account_variables>);
    };

    struct client_variables
    {
        client_variables(nextgen::network::service network_service) : network_service(network_service), client(network_service), agent(nextgen::null)
        {

        }

        nextgen::network::service network_service;
        nextgen::network::http_client client;
        nextgen::network::http_agent agent;
    };

    class client
    {
        public: void view_video(video v1, size_t view_max) const
        {
            auto self = *this;

            if(self->agent == 0)
                self->agent = nextgen::network::http_agent("Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9.1.5) Gecko/20091109 Ubuntu/9.10 (karmic) Firefox/3.5.5");

            if(YOUTUBE_DEBUG_1)
                std::cout << "user agent: " << self->agent->title << std::endl;

            self->client.connect("www.youtube.com", 80,
            [=]()
            {
                if(YOUTUBE_DEBUG_1)
                    std::cout << "[proxos:youtube] Connected." << std::endl;

                nextgen::network::http_message m1;

                m1->method = "GET";
                m1->url = "http://www.youtube.com/watch?v=" + v1->id;
                m1->header_list["Host"] = "www.youtube.com";
                m1->header_list["User-Agent"] = self->agent->title;
                m1->header_list["Keep-Alive"] = "300";
                m1->header_list["Connection"] = "keep-alive";

                self->client.send_and_receive(m1, [=](nextgen::network::http_message r1)
                {
                    if(YOUTUBE_DEBUG_1)
                        std::cout << "[proxos:youtube] Received video page response." << std::endl;

                    if(YOUTUBE_DEBUG_1)
                        std::cout << "c_length " << r1->content.length() << std::endl;

                        //std::cout << "c_length " << r1->content << std::endl;

                        std::cout << "stat " << r1->status_code << std::endl;

                        std::cout << "cookies " << r1->header_list["set-cookie"] << std::endl;

                    if(r1->status_code != 200
                    || r1->header_list["set-cookie"].find("youtube.com") == std::string::npos)
                    {
                        if(YOUTUBE_DEBUG_1)
                            std::cout << "[proxos:youtube] Error receiving video page. " << r1->status_code << std::endl;

                        return;
                    }

                    std::string token = nextgen::preg_match("\"t\"\\: \"(.+?)\"\\,", r1->content);
                    std::string fmt = nextgen::preg_match("\\%2C([0-9]+)\\%7Chttp", r1->content);

                    if(token == nextgen::null_str
                    || fmt == nextgen::null_str)
                    {
                        std::cout << "[youtube] error: null token" << std::endl;
                        std::cout << token << std::endl;
                        std::cout << fmt << std::endl;

                        std::cout << "c_content " << r1->content << std::endl;

                        return;
                    }

                    nextgen::network::http_message m3;

                    m3->method = "GET";
                    m3->url = "http://www.youtube.com/get_video?ptk=wmg&fmt=" + fmt + "&asv=3&video_id=" + v1->id + "&el=detailpage&t=" + token + "&noflv=1";

                    //m3->url = "http://www.youtube.com/get_video?video_id=" + v1->id + "&t=" + token + "&el=detailpage&ps=";//&noflv=1";
                    m3->header_list["Host"] = "www.youtube.com";
                    m3->header_list["User-Agent"] = self->agent->title;
                    m3->header_list["Keep-Alive"] = "300";
                    m3->header_list["Connection"] = "keep-alive";
                    m3->header_list["Cookie"] = r1->header_list["set-cookie"];

                    if(YOUTUBE_DEBUG_1)
                        std::cout << "[youtube] Receiving download" << std::endl;

                    if(r1->header_list["proxy-connection"] == "close"
                    || r1->header_list["connection"] == "close")
                    // reconnect if proxy closes the connection after one HTTP request
                    {
                        self->client.reconnect([=]()
                        {
                            self.video_download_detail(v1, m3, 0, view_max);
                        });

                        return;
                    }

                    self.video_download_detail(v1, m3, 0, view_max);
                });
            });
        }

        private: void video_download_detail(video v1, nextgen::network::http_message m3, size_t view_count, size_t view_max) const
        {
            auto self = *this;

            if(view_count < view_max)
            {
                nextgen::timeout(self->network_service, [=]()
                {
                    self->client.send_and_receive(m3, [=](nextgen::network::http_message r3)
                    {
                        if((r3->status_code != 204)
                        || nextgen::to_int(r3->header_list["content-length"]) != 0)
                        {
                            if(YOUTUBE_DEBUG_1)
                                std::cout << "[proxos:youtube] Error receiving video download. " << r3->status_code << std::endl;

                            return;
                        }

                        ++v1->session_views;

                        if(YOUTUBE_DEBUG_1)
                            std::cout << "[proxos::youtube] VIEWED " << view_count+1 << " TIMES (TOTAL = " << v1->session_views << ")" << std::endl;

                        if(r3->header_list["proxy-connection"] == "close"
                        || r3->header_list["connection"] == "close")
                        // reconnect if proxy closes the connection after one HTTP request
                        {
                            self->client.reconnect([=]()
                            {
                                self.video_download_detail(v1, m3, view_count+1, view_max);
                            });

                            return;
                        }

                        self.video_download_detail(v1, m3, view_count+1, view_max);
                    });
                }, nextgen::random(1000, 3000));
            }
        }

        public: void create_account(account a1, std::function<void()> successful_handler) const
        {
            auto self = *this;

            self->client->proxy = nextgen::network::http_proxy("85.185.25.250", 3128);
            self->client.connect("www.google.com", 80, //"219.93.178.162", 3128),
            [=]
            {
                nextgen::network::http_message m1;

                m1->method = "GET";
                m1->url = "https://www.google.com/accounts/NewAccount";
                m1->header_list["Host"] = "www.google.com";
                m1->header_list["User-Agent"] = "Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9.1.5) Gecko/20091109 Ubuntu/9.10 (karmic) Firefox/3.5.5";
                m1->header_list["Keep-Alive"] = "300";
                m1->header_list["Connection"] = "keep-alive";

                self->client.send_and_receive(m1, [=](nextgen::network::http_message r1)
                {
                    if(r1->status_code != 200)
                    {
                        std::cout << "failed to receive signup page" << r1->status_code << std::endl;

                        return;
                    }

                    std::cout << "new_account response: " << std::endl;


                    std::string ctoken = nextgen::preg_match("accounts/Captcha\\?ctoken=([^\"<>]+)\"", r1->content);
                    std::string dsh = nextgen::preg_match("id=\"dsh\"[^<>]+value=\"([^\"<>]+)\"", r1->content);
                    std::string newaccounttoken = nextgen::preg_match("id=\"newaccounttoken\"[^<>]+value=\"([^\"<>]+)\"", r1->content);
                    std::string newaccounturl = nextgen::preg_match("id=\"newaccounturl\"[^<>]+value=\"([^\"<>]+)\"", r1->content);
                    std::string newaccounttoken_audio = nextgen::preg_match("id=\"newaccounttoken_audio\"[^<>]+value=\"([^\"<>]+)\"", r1->content);
                    std::string newaccounturl_audio = nextgen::preg_match("id=\"newaccounturl_audio\"[^<>]+value=\"([^\"<>]+)\"", r1->content);
                    std::string location = nextgen::preg_match("value=\"([^\"<>]+)\"[^<>]+selected", r1->content);

std::ofstream f;
f.open("google1.html", std::ios::out | std::ios::binary);

if(f.is_open())
{
    nextgen::find_and_replace(r1->content, "http", "hxxp");
    f << r1->content;
}

std::cout << "email: " << a1->user->email.to_string() << std::endl;
std::cout << "password: " << a1->user->password << std::endl;

                    m1->method = "GET";
                    m1->referer = m1->url;
                    m1->url = "https://www.google.com/accounts/Captcha?ctoken=" + ctoken;
                    m1->header_list["Referer"] = m1->referer;

                    std::cout << m1->url << std::endl;

                    self->client.send_and_receive(m1,
                    [=](nextgen::network::http_message r2)
                    {
                        std::cout << "got captcha" << std::endl;

std::ofstream f;
f.open("captcha.jpeg", std::ios::out | std::ios::binary);

if(f.is_open())
{

    f << r2->content;
}

                        std::string newaccountcaptcha;

                        std::getline(std::cin, newaccountcaptcha);

                        std::string ktl = "";

                        std::getline(std::cin, ktl);

                        m1->content = "dsh=" + nextgen::url_encode(dsh)
                        + "&ktl=" + nextgen::url_encode(ktl)
                        + "&ktf=Email+Passwd+PasswdAgain+newaccountcaptcha+"
                        + "&Email=" + nextgen::url_encode(a1->user->email.to_string())
                        + "&Passwd=" + nextgen::url_encode(a1->user->password)
                        + "&PasswdAgain=" + nextgen::url_encode(a1->user->password)
                        + "&PersistentCookie=yes&rmShown=1&smhck=1&nshk=1&loc=" + location
                        + "&newaccounttoken=" + nextgen::url_encode(newaccounttoken)
                        + "&newaccounturl=" + nextgen::url_encode(newaccounturl)
                        + "&newaccounttoken_audio=" + nextgen::url_encode(newaccounttoken_audio)
                        + "&newaccounturl_audio=" + nextgen::url_encode(newaccounturl_audio)
                        + "&newaccountcaptcha=" + nextgen::url_encode(newaccountcaptcha)
                        + "&privacy_policy_url=http%3A%2F%2Fwww.google.com%2Fintl%2Fen%2Fprivacy.html"
                        + "&requested_tos_location=" + location
                        + "&requested_tos_language=en&served_tos_location=" + location
                        + "&served_tos_language=en&submitbutton=I+accept.+Create+my+account.";

                        std::cout << m1->content << std::endl;

                        m1->method = "POST";
                        m1->url = "https://www.google.com/accounts/CreateAccount";

                        self->client.send(m1, [=]
                        {
                            a1->user->email.receive([=](std::string content)
                            {
                                std::string c = nextgen::preg_match("accounts/VE\\?c\\=(.+?)\\&hl\\=en", content);

                                if(c == nextgen::null_str)
                                {
                                    std::cout << "[youtube] error: null c" << std::endl;

                                    return;
                                }

                                nextgen::network::http_client c2(self->network_service);

                                nextgen::network::http_message m2;

                                m2->method = "GET";
                                m2->url = "https://www.google.com/accounts/VE?c=" + c + "&hl=en";
                                m2->header_list["Host"] = "www.google.com";
                                m2->header_list["User-Agent"] = "Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9.1.5) Gecko/20091109 Ubuntu/9.10 (karmic) Firefox/3.5.5";
                                m2->header_list["Keep-Alive"] = "300";
                                m2->header_list["Connection"] = "keep-alive";

                                c2.send_and_receive(m2,
                                [=](nextgen::network::http_message r2)
                                {

                                    if(successful_handler != nextgen::null)
                                        successful_handler();
                                });
                            });

                            self->client.receive([=](nextgen::network::http_message r3)
                            {
                                std::cout << "create_account response: " << std::endl;
std::ofstream f;
f.open("google2.html", std::ios::out | std::ios::binary);

if(f.is_open())
{
    nextgen::find_and_replace(r3->content, "http", "hxxp");
    f << r3->content;
}
                                self->client.disconnect();
                            });
                        });
                    });
                });
            });
        }

        NEXTGEN_ATTACH_SHARED_VARIABLES(client, client_variables);
    };
}

#endif
