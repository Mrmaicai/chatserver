#include "json.hpp"
using json = nlohmann::json; // 别名

#include<iostream>
#include<vector>
#include<map>
#include<string>
using namespace std;

// json 序列化示例
void func1(){
    json js;
    // 类似键值对类型的容器
    js["msg_type"] = 2;
    js["from"] = "zhang san";
    js["to"] = "li si";
    js["msg"] = "hello,what are you doing now?";

    string sendBuf = js.dump(); // json 序列转字符串
    cout<< sendBuf.c_str()<<endl; // 字符串转char* 输出
    // cout<<js<<endl; // 直接输出json串
}

// json 序列化示例2
void func2(){
    json js;
    js["id"] = {1,2,3,4,5};
    js["msg"]["zhang san"] = "hello world";
    js["msg"]["liu shuo"] = "hello china";
    // 上面等同于下面这句一次性添加数组对象
    // js["msg"] = {{"zhang san", "hello world"},{"liu shuo","hello china"}};

    cout << js << endl;

}

// 序列化容器
void func3(){
    json js;
    vector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);

    js["list"] = vec;

    map<int,string> m;
    m.insert({1,"黄山"});
    m.insert({2,"华山"});
    m.insert({3,"泰山"});

    js["path"] = m;
    
    cout << js << endl;
}


int main() {
    func3();
    
    return 0;
}
