#include <cstdio>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/uio.h>
#include <sstream>
#include <thread>
#include <vector>
#include <mariadb/conncpp.hpp>

#define BUF_SIZE 100
#define MAX_CLNT 256
#define PORT 7874

#define SIZE 1

void *handle_clnt(void *arg);
void error_handling(const std::string &msg);

int clnt_cnt = 0;
int clnt_socks[MAX_CLNT];
int clnt_socks1[2];
std::vector<int> clnt_socks2;
std::vector<int>RandRoom;

pthread_mutex_t mutx;
std::thread chatThread;

void send_data(struct iovec *iov, std::string f_send_string, int clnt_sock, int size);
std::string recv_data(struct iovec *iov, std::string f_recv_string, int clnt_sock, int size);

class DB{
private:
    sql::Connection* m_connection;
public:

    sql::PreparedStatement* prepareStatement(const std::string& query)
    {
        sql::PreparedStatement* stmt(m_connection->prepareStatement(query));
        return stmt;
    }

	sql::Statement* createStatement()
	{
        sql::Statement* stmt(m_connection->createStatement());
		return stmt;
	}

    void connect()
    {
        try
        {
            sql::Driver* driver = sql::mariadb::get_driver_instance();
            sql::SQLString url = "jdbc:mariadb://10.10.21.114:3306/chat";
            sql::Properties properties({{"user", "OPERATOR"}, {"password", "1234"}});
            m_connection = driver->connect(url,properties);
            std::cout << "DB 접속\n";  
        }
        catch(sql::SQLException& e){ std::cerr<<"DB 접속 실패: " << e.what() << std::endl;}
    }
    ~DB(){std::cout << "DB 접속 종료\n";}
};

class Member{
private:
    DB& mdb;
	int PortNum;
    int socketnum;
    std::string ID;
    std::string Password;
    std::string NickName;
    std::string onoff;
    std::string chat_Nick;

    int fri_socket;
    int random;
    int a;
    int chatOnOff;
    
    std::vector<std::vector<std::string>> f_list;
    std::vector<std::string> ve_list;
    std::vector<int> chats;

public:
    Member(DB& db) : mdb(db){}

    void OutputPort()
    {
            sql::PreparedStatement* PortN = mdb.prepareStatement("select max(PortNum) + 1 from my");
            sql::ResultSet* res = PortN->executeQuery();
            while(res->next())
                PortNum = res->getInt(1);
            std::cout<< "생성 고유 번호 :" << PortNum<<std::endl;
    }
 
    void sign_up(int clnt_sock, struct iovec *iov)
    {
        OutputPort();
        std::string inputData[3] = {"[ 아이디 ] 입력 ", "[ 비밀번호 ] 입력", "[ 닉네임 ] 입력"};
        std::string receivedData[3];

        while (1)
        {
            for (int i = 0; i < 3; i++)
            {
                send_data(iov, inputData[i], clnt_sock, SIZE);
                receivedData[i] = recv_data(iov, receivedData[i], clnt_sock, SIZE);
            }

            sql::PreparedStatement *stmnt = mdb.prepareStatement("SELECT ID FROM my WHERE ID = ?");
            stmnt->setString(1, receivedData[0]);

            sql::ResultSet *res = stmnt->executeQuery();

            if (res->rowsCount() == 0)
            {
                sql::PreparedStatement *stmnt1 = mdb.prepareStatement("INSERT INTO my (PortNum, ID, Password, NickName) VALUES(?, ?, ?, ?)");
                stmnt1->setInt(1, PortNum);
                stmnt1->setString(2, receivedData[0]);
                stmnt1->setString(3, receivedData[1]);
                stmnt1->setString(4, receivedData[2]);

                stmnt1->executeUpdate();

                send_data(iov, ">> 회원가입이 완료되었습니다.\n>> 회원님의 고유번호는 [" + std::to_string(PortNum) + "] 입니다.\n", clnt_sock, SIZE);
                break;
            }
            else
            {
                send_data(iov, ">> 이미 사용 중인 아이디입니다. 다른 아이디를 입력해주세요.\n=================================", clnt_sock, SIZE);
                continue;
            }
        }
    }

    void Showfriend(int clnt_sock, struct iovec *iov)
    {
        std::string list;
        ve_list.clear();
        f_list.clear();

        sql::PreparedStatement *F_list = mdb.prepareStatement("SELECT * FROM friend WHERE My_Num = ? AND state = 2;");
        F_list->setInt(1, PortNum);

        sql::ResultSet *resF = F_list->executeQuery();

        if(resF->rowsCount() != 0)
        {
            while(resF->next())
            {
                list += "["+(std::string)resF->getString(3) + "] 님(" + (std::string)resF->getString(4) + ")\n";
                ve_list.emplace_back(std::to_string(resF->getInt(6)));
                ve_list.emplace_back(resF->getString(3));
                ve_list.emplace_back(resF->getString(4));   

                f_list.emplace_back(ve_list);
                ve_list.clear();
            }
             
            send_data(iov, list, clnt_sock, SIZE);

            for (int i = 0; i <f_list.size();i++)
            {
                for(int j = 0; j <f_list[i].size();j++)
                    std::cout<<f_list[i][j]<<" / ";
                std::cout<<std::endl; 
            }
        }
        else
            send_data(iov, ">> 등록된 친구가 없습니다.\n", clnt_sock, SIZE);
    }

    void login(int clnt_sock, struct iovec *iov)
    {
        std::string inputData[2] = {"[ 아이디 ] 입력 ", "[ 비밀번호 ] 입력 "};
        std::string receivedData[2];
        std::string inputport[1];
        std::string port;
        std::string idpw;
        int count = 0;

        while (1)
        {
            for (int j = 0; j < 2; j++)
            {
                send_data(iov, inputData[j], clnt_sock, SIZE);
                receivedData[j] = recv_data(iov, receivedData[j], clnt_sock, SIZE);
            }
            while (count >= 5)
            {
                for (int j = 0; j < 2; j++)
                {
                    send_data(iov, inputData[j], clnt_sock, SIZE);
                    receivedData[j] = recv_data(iov, receivedData[j], clnt_sock, SIZE);
                }
                send_data(iov, "[ 고유 번호 ] 입력", clnt_sock, SIZE);
                port = recv_data(iov, inputport[0], clnt_sock, SIZE);

                sql::PreparedStatement *stmntPort = mdb.prepareStatement("SELECT PortNum FROM my WHERE PortNum = ?");
                stmntPort->setInt(1, stoi(port));

                sql::ResultSet *resPort = stmntPort->executeQuery();
                if (resPort->rowsCount() == 1)
                {
                    std::cout << "로그인";
                    break;
                }
                else
                {
                    send_data(iov, ">> 고유번호 일치하지 않습니다.\n", clnt_sock, SIZE);
                    continue;
                }
            }
            sql::PreparedStatement *stmntID = mdb.prepareStatement("SELECT ID FROM my WHERE ID = ?");
            stmntID->setString(1, receivedData[0]); 

            sql::ResultSet *resID = stmntID->executeQuery();
            if (resID->rowsCount() == 1)
            {
                sql::PreparedStatement *stmntPW = mdb.prepareStatement("SELECT * FROM my WHERE Password = ?");
                stmntPW->setString(1, receivedData[1]); 

                sql::ResultSet *resPW = stmntPW->executeQuery();
                if (resPW->rowsCount() == 1)
                {
                    send_data(iov, ">> 로그인 되었습니다.\n=================================", clnt_sock, SIZE);

                    while (resPW->next())
                    {
                        PortNum = PortNum = resPW->getInt(1);
                        ID = resPW->getString(2);
                        Password = resPW->getString(3);
                        NickName = resPW->getString(4);
                        socketnum = resPW->getInt(6);

                        chat_Nick = "["+NickName+"]";
                        send_data(iov, chat_Nick, clnt_sock, SIZE);

                        std::cout << "[접속자]\nID : " << ID << "\nNickName : " << NickName << "\nPortNum : " << PortNum << std::endl;
                    }

                   
                    sql::PreparedStatement *stmntUSER = mdb.prepareStatement("UPDATE my SET OnOff = '온라인', socketnum = ? WHERE ID = ?");
                    stmntUSER->setInt(1, clnt_sock);
                    stmntUSER->setString(2, receivedData[0]);
                    stmntUSER->executeQuery();

                    sql::PreparedStatement *stmntFRI = mdb.prepareStatement("UPDATE friend SET fri_OnOff = '온라인', fri_socknum = ? WHERE fri_num = ?");
                    stmntFRI->setInt(1, clnt_sock);
                    stmntFRI->setInt(2, PortNum);
                    stmntFRI->executeQuery();

                    break;
                }

                else if (resPW->rowsCount() != 1)
                {
                    send_data(iov, ">> 비밀번호가 일치하지 않습니다.\n", clnt_sock, SIZE);
                    count += 1; 
                }
            }
            else
            {
                send_data(iov, ">> 일치하는 정보가 없습니다.\n", clnt_sock, SIZE);
                count += 1; 
            }
        }
    }

    void RequestFriend(int clnt_sock, struct iovec *iov)  
    {
        int F_port, F_sock =0;
        std::string F_ID, F_Nick, F_OnOff;

        send_data(iov, ">> 요청할 친구의 고유번호를 입력하세요: ", clnt_sock, SIZE);
        std::string requestdata[1];
        requestdata[0] = recv_data(iov, requestdata[0], clnt_sock, SIZE); 
        F_port = stoi(requestdata[0]);

        sql::PreparedStatement *stmntMember = mdb.prepareStatement("SELECT * FROM my WHERE PortNum = ?");    
        stmntMember->setInt(1, F_port);

        sql::ResultSet *resMem = stmntMember->executeQuery();
        if (resMem->rowsCount() == 1)   
        {
            sql::PreparedStatement *stmnt_F = mdb.prepareStatement("SELECT * FROM friend WHERE My_Num = ? AND fri_Num = ?");
            stmnt_F->setInt(1, PortNum);
            stmnt_F->setInt(2, F_port);

            sql::ResultSet *resFri = stmnt_F->executeQuery();
            if (resFri->rowsCount() == 0)   
            {
                while (resMem->next())
                {
                    F_port = resMem->getInt(1);
                    F_ID = resMem->getString(2);
                    F_Nick = resMem->getString(4);
                    F_OnOff = resMem->getString(5);
                    F_sock = resMem->getInt(6);
                }

                sql::PreparedStatement *plusMy = mdb.prepareStatement("INSERT INTO friend(My_Num, fri_Num, fri_Nick, state) VALUES(?, ?, ? ,?)");
                plusMy->setInt(1, PortNum);
                plusMy->setInt(2, F_port);
                plusMy->setString(3, F_Nick);
                plusMy->setInt(4, 0);

                plusMy->executeQuery();

                sql::PreparedStatement *plusFri = mdb.prepareStatement("INSERT INTO friend(My_Num, fri_Num, fri_Nick, state) VALUES(?, ?, ? ,?)");
                plusFri->setInt(1, F_port);
                plusFri->setInt(2, PortNum);
                plusFri->setString(3, NickName);
                plusFri->setInt(4, 1);

                plusFri->executeQuery();

                send_data(iov, ">> 친구 요청이 완료되었습니다.\n=================================", clnt_sock, SIZE);
            }
            else
            {
                send_data(iov, ">> 현재 친구요청이 불가합니다.\n(# 친구 요청을 받은 상태, 친구 요청을 한 상태 또는 친구인 상태 일 수 있습니다.)\n=================================", clnt_sock, SIZE);
            }
        }
        else
        {
            send_data(iov, ">> 회원 정보가 없습니다.\n=================================", clnt_sock, SIZE);
        }
        std::cout << std::endl;
    }

    void plusfriend(int clnt_sock, struct iovec *iov)   
    {
        int F_Port = 0;
        std::string list;
        std::string plus_string;

        sql::PreparedStatement *Rq_Friend = mdb.prepareStatement("SELECT * FROM friend WHERE My_Num = ? AND state = 1;");
        Rq_Friend->setInt(1, PortNum);

        sql::ResultSet *resRq = Rq_Friend->executeQuery();
        if (resRq->rowsCount() != 0)
        {
            while (resRq->next())
            {
                std::cout << "NickName : " << resRq->getString(3) << " / 상태 : " << resRq->getString(4) << " / PortNum : " << resRq->getInt(2) << std::endl;
                list += "NO." + (std::string)resRq->getString(2) + "  [" + (std::string)resRq->getString(3) + "] 님(" + (std::string)resRq->getString(4) + ")\n";
            }
            send_data(iov, list, clnt_sock, SIZE);
            sleep(1);

            send_data(iov, ">> 친구 번호를 입력시면 친구요청을 (수락 또는 거절) 가능합니다.\n", clnt_sock, SIZE);
            plus_string = recv_data(iov, plus_string, clnt_sock, SIZE); 
            F_Port = stoi(plus_string);

            sql::PreparedStatement *Choice = mdb.prepareStatement("SELECT * FROM friend WHERE My_Num = ? AND fri_Num = ?");
            Choice->setInt(1, PortNum);
            Choice->setInt(2, F_Port);

            sql::ResultSet *resCh = Choice->executeQuery();
            if (resCh->rowsCount() == 1)
            {
                send_data(iov, ">> 친구 요청을 수락하시겠습니까? [수락 : Y / 거절 : N]\n", clnt_sock, SIZE);

                plus_string = recv_data(iov, plus_string, clnt_sock, SIZE);

                if (plus_string == "Y" || plus_string == "y")
                {
                    send_data(iov, ">> 수락 완료되었습니다.\n=================================", clnt_sock, SIZE);

                    sql::PreparedStatement *plusup1 = mdb.prepareStatement("UPDATE friend SET state = 2 WHERE My_Num = ? AND fri_Num = ?");
                    plusup1->setInt(1, PortNum);
                    plusup1->setInt(2, F_Port);
                    plusup1->executeQuery();

                    sql::PreparedStatement *plusup2 = mdb.prepareStatement("UPDATE friend SET state = 2 WHERE My_Num = ? AND fri_Num = ?");
                    plusup2->setInt(1, F_Port);
                    plusup2->setInt(2, PortNum);
                    plusup2->executeQuery();
                }
                else
                {
                    send_data(iov, ">> 거절 완료되었습니다.\n=================================", clnt_sock, SIZE);
                    sql::PreparedStatement *minudel1 = mdb.prepareStatement("delete from friend WHERE My_Num = ? AND fri_Num = ?");
                    minudel1->setInt(1, PortNum);
                    minudel1->setInt(2, F_Port);
                    minudel1->executeQuery();

                    sql::PreparedStatement *minudel2 = mdb.prepareStatement("delete from friend WHERE My_Num = ? AND fri_Num = ?");
                    minudel2->setInt(1, F_Port);
                    minudel2->setInt(2, PortNum);
                    minudel2->executeQuery();
                }
            }
            else
            {
                send_data(iov, ">> 입력한 번호를 다시 확인해주세요.\n", clnt_sock, SIZE);
            }
        }
        else
        {
            send_data(iov, "[ 받은 친구요청이 없습니다 ]\n=================================", clnt_sock, SIZE);
        }
        std::cout << std::endl;
    }

    int userAdd(int clnt_sock, std::string recv_string)
    {
        int check = 0;
        int friend_socket = 100;
        for (int i = 0; i < f_list.size(); i++)
        {
            for (int j = 0; j < f_list[i].size(); j++)
            {
                if(recv_string == "완료" )
                {   
                    friend_socket = -1;
                    return friend_socket;
                }
                if (recv_string == f_list[i][1])
                {
                    if (f_list[i][2] == "온라인")
                    {
                        std::cout << "닉네임 : " << f_list[i][1] << "/ 소켓 번호: " << f_list[i][0] << std::endl;
                        friend_socket = stoi(f_list[i][0]);
                        return friend_socket;
                    }
                    else
                    {
                        std::cout << "접속한 친구로 만 대화가 가능합니다\n";
                        check = 1;
                    }
                    break; 
                }
            }
            if (check == 1)
            {
                break; 
            }
        }
        if (check == 0)
        {
            std::cout << "친구가 아니다\n";
        }
        std::cout << friend_socket << " : 메롱\n";
        return friend_socket;
    }

    void chat(int clnt_sock, struct iovec *iov )
    {
        int sock_num;
        std::string recv_string;

        send_data(iov, "1. 개인채팅\n2. 단체채팅\n3. 개인채팅방 확인\n4. 단체채팅방 확인\n5. 랜덤채팅\n============================", clnt_sock, SIZE);
        recv_string = recv_data(iov, recv_string, clnt_sock, SIZE);

        if(recv_string == "1")  
        {    
            Showfriend(clnt_sock, iov);
            recv_string= recv_data(iov, recv_string, clnt_sock, SIZE);
            std::cout<<NickName<<" 님이 ["<<recv_string<<"] 님에게 채팅 요청"<<std::endl;
            fri_socket = userAdd(clnt_sock, recv_string);
            std::cout<<"나와따!: "<<fri_socket;
            std::cout<<"\n";

            clnt_socks1[0] = clnt_sock;
            clnt_socks1[1] = fri_socket;
            chat2(clnt_sock, iov);
        }


        else if (recv_string == "2") 
        {
            Showfriend(clnt_sock, iov);
            while (1)
            {
                send_data(iov, ">> 초대할 친구 이름을 입력하세요.[초대가 끝났을 시 '완료'를 입력해주세요]\n=================================", clnt_sock, SIZE);
                recv_string = recv_data(iov, recv_string, clnt_sock, SIZE);
                sock_num = userAdd(clnt_sock, recv_string);
                std::cout << sock_num << " : 추가\n";

                if (sock_num == -1)
                {
                    clnt_socks2.emplace_back(clnt_sock);
                    break;
                }
                clnt_socks2.emplace_back(sock_num);
            }

            for (int i = 0; i < clnt_socks2.size(); i++)
            {
                std::cout << clnt_socks2[i] << " ";
                chats.emplace_back(clnt_socks2[i]);
            }
            std::cout << "나와따!\n";
            send_data(iov, ">> 친구가 열심히 달려오고 있습니다..!\n", clnt_sock, SIZE);
            send_data(iov, Chat_read(clnt_sock, iov, chats), clnt_sock, SIZE);
        }

        else if(recv_string == "3")
        {
            std::cout<<"생성된 채팅방";
            chat3(clnt_sock, iov);
        }
        else if(recv_string == "4")
        {
            std::cout<<"생성된 채팅방";
            for (int i = 0; i <clnt_socks2.size();i++)
            {   
                chats.emplace_back(clnt_socks2[i]);
                std::cout<<clnt_socks2[i]<< " ";
            }
            send_data(iov,Chat_read(clnt_sock, iov, chats), clnt_sock, SIZE);
        }

        else if(recv_string == "5")
        {
            std::cout<<"랜덤 채팅";
            RandomChat(clnt_sock, iov);
        }
    }

    void chat2(int clnt_sock, struct iovec *iov) 
    {
        std::string recv_string;
        std::string send_name = "["+NickName+"]";
        std::cout << "소켓 번호[" << fri_socket << "]과 채팅이 시작됩니다.\n";
        send_data(iov, ">> 친구가 열심히 달려오고 있습니다..!\n", clnt_sock, SIZE);

        while (1) {
            recv_string = recv_data(iov, recv_string, clnt_sock, SIZE);
            if (recv_string == "QUIT") 
            {
                std::cout << ">> 채팅 종료.\n";
                send_data(iov, ">> 상대방이 채팅을 종료했습니다.\n>> 채팅방을 종료하시려면 (QUIT)를 입력해 주세요.\n", fri_socket, SIZE);
                recv_data(iov, recv_string, clnt_sock, SIZE);
                break;
            }
            send_data(iov, send_name+recv_string, fri_socket, SIZE);
            send_data(iov,send_name+ recv_string, clnt_sock, SIZE);
        }
        if(clnt_socks1[0] == clnt_sock)
            clnt_socks1[0] = 0;
        std::cout <<"거는 채팅방이 삭제됩니다.\n";
    }


    std::string Chat_read(int clnt_sock, struct iovec *iov, std::vector<int> &chat_list)
    {
        std::string recv_string;
        std::string send_name = "["+NickName+"]";

        for (int i = 0; i < chat_list.size(); i++)
        {
            send_data(iov, send_name+"님이 채팅방에 입장하셨습니다!\n", chat_list[i], SIZE);
        }
        
        while (1)
        {
            recv_string = recv_data(iov, recv_string, clnt_sock, SIZE);

            if (recv_string == "QUIT")
            {
          
                for (int i = 0; i < chat_list.size(); i++)
                {
                    if (chat_list[i] == clnt_sock)
                    {
                        chat_list.erase(chat_list.begin() + i);
                        send_data(iov, send_name + "님이 채팅방을 나가셨습니다!\n", clnt_sock, SIZE);
                    }
                    else
                    {
                        send_data(iov, send_name + "님이 채팅방을 나가셨습니다!\n", chat_list[i], SIZE);
                    }
                }
                return recv_string;
            }
            else
            {
                for (int i = 0; i < chat_list.size(); i++)
                {
                    send_data(iov, send_name+recv_string, chat_list[i], SIZE);
                }
            }
        }
    }

void chat3(int clnt_sock, struct iovec *iov)
    {
        fri_socket = clnt_socks1[0];
        std::string data, recv_string;
        std::string send_name = "["+NickName+"]";
        send_data(iov, send_name+ "님이 입장했습니다.\n", fri_socket, SIZE);

        int str_len;
        std::cout <<"소켓 번호["<<clnt_socks1[0]<<"]과 채팅이 시작됩니다.\n";
        while (1) 
        {
            recv_string = recv_data(iov, recv_string, clnt_sock, SIZE);
            if (recv_string == "QUIT") 
            {
                std::cout<<"채팅 종료.\n";
                send_data(iov, "상대방이 채팅을 종료했습니다.\n채팅방을 종료하시려면 (QUIT)를 입력해 주세요.\n",fri_socket, SIZE);
                break;
            }
            send_data(iov, send_name + recv_string, fri_socket, SIZE);
            send_data(iov, send_name + recv_string, clnt_sock, SIZE);
        }
        std::cout <<"받는 채팅방이 삭제됩니다.\n"; 
        if(clnt_socks1[1] == clnt_sock)
            clnt_socks1[1] = 0;
    }

    void RandomChat(int clnt_sock, struct iovec *iov)
    {
        std::string recv_string;

        send_data(iov, ">> 랜덤채팅을 시작하시겠습니까? [수락 : Y | 거절 : N]", clnt_sock, SIZE);
        recv_string= recv_data(iov, recv_string, clnt_sock, SIZE);
        std::cout << recv_string << std::endl;

        if(recv_string == "수락" || recv_string == "Y" || recv_string == "y")
        {
            if (RandRoom.size() < 2)
            {
                RandRoom.emplace_back(clnt_sock);
                send_data(iov, ">> 랜덤채팅에 입장했습니다.종료하시려면 (QUIT)를 입력해 주세요.\n", clnt_sock, SIZE);

                send_data(iov,Chat_read(clnt_sock, iov, RandRoom), clnt_sock, SIZE);

            }
            else
            {
                send_data(iov, ">> 잠시 기다려주세요.\n>> 랜덤채팅 상대를 찾는 중 입니다...", clnt_sock, SIZE);

            }
        }
        else
        {
            send_data(iov, ">> 랜덤채팅 진행 거부\n", clnt_sock, SIZE);
        }
    }

    void LogOut(int clnt_sock, struct iovec *iov)
    {
        sql::PreparedStatement*stmnt1 = mdb.prepareStatement("UPDATE my SET OnOff = default, socketnum = default, Chat_On = default where PortNum = ?");
        stmnt1->setInt(1, PortNum);
        stmnt1->executeQuery();

        sql::PreparedStatement*stmnt2 = mdb.prepareStatement("UPDATE friend SET fri_OnOff = default, fri_socknum = default where fri_num = ?");
        stmnt2->setInt(1, PortNum);
        stmnt2->executeQuery();

        send_data(iov, ">> 로그아웃 되었습니다.\n", clnt_sock, SIZE);
    }
};


int main(int argc, char *argv[])
{
    int clnt_sock;
    int serv_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    socklen_t clnt_adr_sz;
    pthread_t t_id;

    pthread_mutex_init(&mutx, NULL);
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(PORT);

    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");
    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    while (1)
    {
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_sz);

        pthread_mutex_lock(&mutx);
        clnt_socks[clnt_cnt++] = clnt_sock;
        pthread_mutex_unlock(&mutx);

        pthread_create(&t_id, NULL, handle_clnt, (void *)&clnt_sock);
        pthread_detach(t_id);
        printf("Connected client IP: %s \n", inet_ntoa(clnt_adr.sin_addr));
    }
    close(serv_sock);
    return 0;
}

void *handle_clnt(void *arg) 
{
    struct iovec iov[1];
    std::string send_string;
    std::string recv_string;
    std::string recv_string1;
    DB db;
    db.connect();
    Member mm(db);
    int clnt_sock = *(int *)arg;
    int ss;
    int login_check = 0;

    while (recv_string != "q" || recv_string != "Q")
    {
        send_data(iov, "1. 로그인\n2. 회원 가입\nQ. 끝내기\n=================================", clnt_sock, SIZE);
        recv_string = recv_data(iov, recv_string, clnt_sock, SIZE); 
        std::cout << recv_string << std::endl;

        if (recv_string == "1")
        {
            mm.login(clnt_sock, iov);
            while(recv_string != "q" || recv_string != "Q")
            {
                send_data(iov, "1. 친구 추가\n2. 친구 목록\n3. 채 팅\n4. 로그아웃\n=================================", clnt_sock, SIZE);
                recv_string1 = recv_data(iov, recv_string1, clnt_sock, SIZE); 
                if (recv_string1 == "1")
                {
                    send_data(iov, "1. 친구 요청\n2. 친구 요청받은 목록\n=================================", clnt_sock, SIZE);
                    recv_string1 = recv_data(iov, recv_string1, clnt_sock, SIZE);

                    if (recv_string1 == "1")
                    {
                        mm.RequestFriend(clnt_sock, iov);
                    }
                    else if (recv_string1 == "2")
                    {
                        mm.plusfriend(clnt_sock, iov);
                    }
                }
                else if (recv_string1 == "2")
                {
                    mm.Showfriend(clnt_sock, iov);
                }
                else if (recv_string1 == "3")
                {
                    mm.chat(clnt_sock, iov);
                }
                else if (recv_string1 == "4")
                {
                    mm.LogOut(clnt_sock, iov);
                    break;
                }
            }
        }
        else if (recv_string == "2")
        {
            mm.sign_up(clnt_sock, iov);
        }
        else if (recv_string == "q" || recv_string == "Q")
        {
            mm.LogOut(clnt_sock, iov);
            exit(1);
        }
        std::cout <<std::endl;
    }

    pthread_mutex_lock(&mutx);
    for (int i = 0; i < clnt_cnt; i++)
    {
        if (clnt_sock == clnt_socks[i])
        {
            while (i < clnt_cnt - 1)
            {
                clnt_socks[i] = clnt_socks[i + 1];
                i++;
            }
            clnt_socks[--clnt_cnt] = 0;
            break;
        }
    }
    pthread_mutex_unlock(&mutx);
    close(clnt_sock);
    return NULL;
}


void send_data(struct iovec *iov, std::string f_send_string, int clnt_sock, int size)
{
    iov[0].iov_base = (void *)f_send_string.data();
    iov[0].iov_len = f_send_string.size();
    writev(clnt_sock, iov, size);
}


std::string recv_data(struct iovec *iov, std::string f_recv_string, int clnt_sock, int size)
{
    int str_len = 0;
    f_recv_string.resize(BUF_SIZE);
    std::string copy_str; 

    iov[0].iov_base = (void *)&f_recv_string[0];
    iov[0].iov_len = f_recv_string.size();

    str_len = readv(clnt_sock, iov, 1);
    if (str_len <= 0)
    {
        std::cerr << "클라이언트 연결에서 오류가 발생" << std::endl;
        close(clnt_sock);
        return "";
    }

    f_recv_string.resize(str_len); 
    copy_str = f_recv_string;

    f_recv_string.clear();
    f_recv_string.resize(BUF_SIZE);

    return copy_str;
}

void error_handling(const std::string &msg) {
    std::cerr << msg << std::endl;
    exit(1);
}

