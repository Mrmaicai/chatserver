#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include <string>
#include <vector>
using namespace muduo;
using namespace std;
// 获取单例对象的接口函数
ChatService* ChatService::instance()
{
    static ChatService service;
    return &service;
}


// 注册消息以及对应的Handler回调操作
ChatService::ChatService()
{
    _msgHandlerMap.insert({LOGIN_MSG,std::bind(&ChatService::login,this,_1,_2,_3)});
    _msgHandlerMap.insert({LOGINOUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG,std::bind(&ChatService::reg,this,_1,_2,_3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG,std::bind(&ChatService::oneChat,this,_1,_2,_3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG,std::bind(&ChatService::addFriend,this,_1,_2,_3)});
    _msgHandlerMap.insert({CREATE_GROUP_MSG,std::bind(&ChatService::createGroup,this,_1,_2,_3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG,std::bind(&ChatService::addGroup,this,_1,_2,_3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG,std::bind(&ChatService::groupChat,this,_1,_2,_3)});

    if(_redis.connect())
    {       
        // 设置消息上报回调
        _redis.init_notify_handler(std::bind(&ChatService::handelRedisSubcribeMessage,this,_1,_2));
    }
}

void ChatService::reset()
{
    // 把online状态的用户，设置为offline
    _userModel.resetState();
}


// 获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid){
    // 记录错误日志，msgid没有对应的事件处理回调
    auto it = _msgHandlerMap.find(msgid);
    if(it == _msgHandlerMap.end()){ 
        // 返回一个默认处理器,空操作
        return [=](const TcpConnectionPtr& conn,json &js, Timestamp time){
            LOG_ERROR << "msgid:" << msgid << " can not find handler!";
        };
    }
    else{
        return  _msgHandlerMap[msgid];
    }
    
}



// 处理登录业务
void ChatService::login(const TcpConnectionPtr& conn,json &js, Timestamp time)
{
   int id = js["id"].get<int>();
   string pwd = js["password"];

   User user = _userModel.query(id);
   if(user.getId() == id && user.getPwd() == pwd){

    if(user.getState() == "online"){
        // 该用户已经登录，不允许重新登录
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 2;
        response["errmsg"] = "this account is using, input another";
        conn->send(response.dump());
    }
    else
    {
        // 登陆成功,记录用户连接信息
        {
            // 使用 std::lock_guard 来自动管理互斥锁 _connMutex 的锁定和解锁。  
            // 当 lock_guard 对象被创建时，它会自动锁定互斥锁，当 lock_guard 对象离开其作用域（即被销毁）时，它会自动解锁互斥锁。  
            // 这确保了 _connMutex 在插入操作期间被锁定，以防止其他线程同时访问和修改 _userConnMap。 
            lock_guard<mutex> lock(_connMutex);
            _userConnMap.insert({id,conn});
        }
        // 向redis订阅消息通道（id）
        _redis.subscribe(id);

        // 更新用户状态
        user.setState("online");
        _userModel.updateState(user);
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        response["name"] = user.getName();


        // 查询改4该用户是否有离线消息
        vector<string> vec = _offlineMsgModel.query(id);
        if(!vec.empty()){
            response["offlinemsg"] = vec;
            // 读取用户的离线消息后，将其删除
            _offlineMsgModel.remove(id);
        }

        // 查询用户好友信息并返回
        vector<User> userVec = _friendModel.query(id);
        if(!userVec.empty()){
            vector<string> vec2;
            for(auto &user : userVec)
            {
                json js;
                js["id"] = user.getId();
                js["name"] = user.getName();
                js["state"] = user.getState();
                vec2.push_back(js.dump());
            }
            response["friends"] = vec2;
        }

        vector<Group> groupVec = _groupModel.queryGroups(id);
        if(!groupVec.empty()){
            vector<string> vec2;
            for(Group &group : groupVec)
            {
                json js;
                js["id"] = group.getId();
                js["groupname"] = group.getName();
                js["groupdesc"] = group.getDesc();
                vector<string> usersVec;
                for(GroupUser &user : group.getUsers())
                {
                    json ujs;
                    ujs["id"] = user.getId();
                    ujs["name"] = user.getName();
                    ujs["state"] = user.getState();
                    ujs["role"] = user.getRole();
                    usersVec.push_back(ujs.dump());
                }
                js["users"] = usersVec;
                vec2.push_back(js.dump());
            }
            response["groups"] = vec2;
        }

        conn->send(response.dump());
    }
    
   }
   else{

    // 改用户不存在， 登录失败
    json response;
    response["msgid"] = LOGIN_MSG_ACK;
    response["errno"] = 1;
    response["errmsg"] = "id or password is invalid!";
    conn->send(response.dump());
   }
}



// 处理注册业务
void ChatService::reg(const TcpConnectionPtr& conn,json &js, Timestamp time)
{   
    string name = js["name"];
    string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPwd(pwd);
    bool state = _userModel.insert(user);
    if(state) // 如果注册成功
    {
        // 创建一个json对象response，用于构建响应数据
        json response;
        response["msgid"] = REG_MSG_ACK;
         // 在response对象中设置"errno"字段的值为0，表示没有错误发生
        response["errno"] = 0;
        response["id"] = user.getId();
        // 调用conn对象的send方法，发送response对象转换成的字符串（通过json库的dump方法）  
        conn->send(response.dump());

    }
    else{
        json response;
        response["msgid"] = REG_MSG_ACK;
        //在response对象中设置"errno"字段的值为1，表示有错误发生
        response["errno"] = 1;
        response["id"] = user.getId();
        conn->send(response.dump());
    }

}

// 注销业务
void ChatService::loginout(const TcpConnectionPtr& conn,json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if(it != _userConnMap.end()){
            _userConnMap.erase(it);
        }
    }
    // 用户下线取消订阅通道
    _redis.unsubscribe(userid);

    User user(userid,"","","offline");
    _userModel.updateState(user);

}

// 处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    User user;
    {
        lock_guard<mutex> lock(_connMutex);
    
        for(auto it = _userConnMap.begin(); it != _userConnMap.end(); ++it)
        {
            if(it->second == conn)
            {   
                // 从map表删除用户的链接信息
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }

    _redis.unsubscribe(user.getId());
    // 更新状态
    if(user.getId() != -1)
    {
        user.setState("offline");
        _userModel.updateState(user);
    }
    
}


// 一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr& conn,json &js, Timestamp time)
{
    int toid = js["toid"].get<int>();

    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if(it != _userConnMap.end())
        {
            // toid在线，转发消息  服务器主动推送消息给toid
            it->second->send(js.dump());
            return;
        }
    }


    // 查询toid是否在线
    User user = _userModel.query(toid);
    if(user.getState() == "online") // 在线，但不在同一服务器上
    {   
        // 在Redis 上发布消息
        _redis.publish(toid,js.dump());
        return;
    }


    // toid不在线，存储离线消息
    _offlineMsgModel.insert(toid,js.dump());
}

// 添加好友业务 msgid id friendid
void ChatService::addFriend(const TcpConnectionPtr& conn,json &js, Timestamp time)
{   
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    // 存储好友信息
    _friendModel.insert(userid,friendid);
}




    // 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr& conn,json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    // 存储新创建的群组信息
    Group group(-1,name,desc);
    if(_groupModel.createGroup(group))
    {
        // 存储群组创建人信息
        _groupModel.addGroup(userid,group.getId(),"creator");
    }
    
}
    // 加入群组业务
void ChatService::addGroup(const TcpConnectionPtr& conn,json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupModel.addGroup(userid,groupid,"normal");
}
    // 群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr& conn,json &js, Timestamp time)
{

    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    // 查询群成员id
    vector<int> useridVec = _groupModel.queryGroupUsers(userid,groupid);
    
    lock_guard<mutex> lock(_connMutex); // 发消息过程上锁
    for(int id : useridVec)  // 遍历每一个群成员id，发送消息
    {
        auto it = _userConnMap.find(id);
        if(it != _userConnMap.end()) // 如果该用户在线
        {
            // 转发群消息
            it->second->send(js.dump());
        }
        else{
            // 查询toid是否在线
            User user = _userModel.query(id);
            if(user.getState() == "online") // 在线，但不在同一服务器上
            {   
                // 在Redis 上发布消息
                _redis.publish(id,js.dump());
            }
            else{
            // 存储离线群消息
            _offlineMsgModel.insert(id,js.dump());
            }
        }
        
    }
}


  // redis
void ChatService::handelRedisSubcribeMessage(int userid ,string msg)
{
    // 确保访问_userConnMap线程安全
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if(it != _userConnMap.end())
    {
        it->second->send(msg);
        return;
    }

    _offlineMsgModel.insert(userid,msg);

}
    