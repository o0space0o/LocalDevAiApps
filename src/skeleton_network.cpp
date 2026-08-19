#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "skeleton_network.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <algorithm>
#include <charconv>
#include <cstring>

namespace skeletonnet {
namespace {
constexpr std::uint32_t WireMagic = 0x314C4B53; // SKL1
constexpr std::uint16_t WirePose = 1;
constexpr std::size_t IdCapacity = 40;
constexpr std::size_t NameCapacity = 48;
constexpr std::uintptr_t InvalidSocketStorage = ~std::uintptr_t{0};
#pragma pack(push, 1)
struct WireVector3 { float x, y, z; };
struct WireFrame {
    std::uint32_t magic; std::uint16_t version; std::uint16_t type; std::uint32_t byte_size;
    std::uint64_t sequence; char source_id[IdCapacity]; char display_name[NameCapacity];
    WireVector3 joints[space3d::HumanoidJointCount];
};
#pragma pack(pop)

SOCKET ToSocket(std::uintptr_t value) { return static_cast<SOCKET>(value); }
std::uintptr_t FromSocket(SOCKET value) { return static_cast<std::uintptr_t>(value); }
struct WinsockRuntime {
    bool ok = false;
    WinsockRuntime() { WSADATA data{}; ok = WSAStartup(MAKEWORD(2, 2), &data) == 0; }
    ~WinsockRuntime() { if (ok) WSACleanup(); }
};
WinsockRuntime& GlobalWinsock() { static WinsockRuntime runtime; return runtime; }

bool SendAll(SOCKET socket, const char* data, int bytes) {
    for (int sent = 0; sent < bytes;) { int n = send(socket, data + sent, bytes - sent, 0); if (n <= 0) return false; sent += n; }
    return true;
}
bool ReceiveAll(SOCKET socket, char* data, int bytes) {
    for (int received = 0; received < bytes;) { int n = recv(socket, data + received, bytes - received, 0); if (n <= 0) return false; received += n; }
    return true;
}
void CopyText(char* destination, std::size_t capacity, const std::string& value) {
    std::memset(destination, 0, capacity); const auto count = (std::min)(capacity - 1, value.size()); std::memcpy(destination, value.data(), count);
}
WireFrame Encode(const SkeletonFrame& frame) {
    WireFrame wire{}; wire.magic=WireMagic; wire.version=ProtocolVersion; wire.type=WirePose; wire.byte_size=sizeof(WireFrame); wire.sequence=frame.sequence;
    CopyText(wire.source_id,sizeof(wire.source_id),frame.source_id); CopyText(wire.display_name,sizeof(wire.display_name),frame.display_name);
    for(std::size_t i=0;i<frame.joints.size();++i) wire.joints[i]={frame.joints[i].x,frame.joints[i].y,frame.joints[i].z}; return wire;
}
bool Decode(const WireFrame& wire, SkeletonFrame& frame) {
    if(wire.magic!=WireMagic||wire.version!=ProtocolVersion||wire.type!=WirePose||wire.byte_size!=sizeof(WireFrame)||wire.source_id[0]=='\0') return false;
    frame.source_id.assign(wire.source_id,strnlen_s(wire.source_id,sizeof(wire.source_id))); frame.display_name.assign(wire.display_name,strnlen_s(wire.display_name,sizeof(wire.display_name))); frame.sequence=wire.sequence;
    for(std::size_t i=0;i<frame.joints.size();++i) frame.joints[i]={wire.joints[i].x,wire.joints[i].y,wire.joints[i].z}; return true;
}
void CloseSocket(std::uintptr_t& storage) { if(storage!=InvalidSocketStorage){ SOCKET s=ToSocket(storage); shutdown(s,SD_BOTH); closesocket(s); storage=InvalidSocketStorage; } }
std::string SocketError(const char* operation) { return std::string(operation)+" failed (Winsock "+std::to_string(WSAGetLastError())+")"; }
}

bool ParseEndpoint(const std::string& url, Endpoint& endpoint, std::string& error) {
    std::string value=url; const auto scheme=value.find("://");
    if(scheme!=std::string::npos){ const auto protocol=value.substr(0,scheme); if(protocol!="tcp"&&protocol!="http"&&protocol!="ws"){error="Only tcp://, http://, or ws:// endpoints are supported (TLS is not enabled).";return false;} value.erase(0,scheme+3); }
    const auto slash=value.find('/'); if(slash!=std::string::npos)value.resize(slash); if(value.empty()){error="Backend host is empty.";return false;} endpoint={};
    auto parse_port=[&](const std::string& text)->bool{ unsigned port=0; auto r=std::from_chars(text.data(),text.data()+text.size(),port); if(r.ec!=std::errc{}||r.ptr!=text.data()+text.size()||port==0||port>65535)return false; endpoint.port=static_cast<std::uint16_t>(port); return true; };
    if(value.front()=='['){ auto end=value.find(']'); if(end==std::string::npos){error="Invalid IPv6 endpoint.";return false;} endpoint.host=value.substr(1,end-1); if(end+1<value.size()&&(value[end+1]!=':'||!parse_port(value.substr(end+2)))){error="Invalid endpoint port.";return false;} }
    else { auto colon=value.rfind(':'); if(colon!=std::string::npos&&value.find(':')==colon){endpoint.host=value.substr(0,colon);if(endpoint.host.empty()||!parse_port(value.substr(colon+1))){error="Invalid endpoint host or port.";return false;}} else endpoint.host=value; }
    error.clear(); return true;
}
SkeletonFrame MakeFrame(const std::string& source_id,const std::string& display_name,std::uint64_t sequence,const space3d::HumanoidPose& pose){SkeletonFrame frame;frame.source_id=source_id;frame.display_name=display_name;frame.sequence=sequence;frame.joints=pose.joints;return frame;}

struct SkeletonServer::Peer { std::uintptr_t socket=InvalidSocketStorage; std::mutex send_mutex; std::thread thread; std::atomic<bool> active{true}; };
SkeletonServer::SkeletonServer()=default; SkeletonServer::~SkeletonServer(){Stop();}
bool SkeletonServer::Start(std::uint16_t port,std::string& error){
    Stop(); if(!GlobalWinsock().ok){error="WSAStartup failed.";return false;} SOCKET listener=socket(AF_INET6,SOCK_STREAM,IPPROTO_TCP); if(listener==INVALID_SOCKET){error=SocketError("socket");return false;}
    DWORD dual=0;setsockopt(listener,IPPROTO_IPV6,IPV6_V6ONLY,reinterpret_cast<const char*>(&dual),sizeof(dual)); BOOL reuse=TRUE;setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,reinterpret_cast<const char*>(&reuse),sizeof(reuse));
    sockaddr_in6 address{};address.sin6_family=AF_INET6;address.sin6_addr=in6addr_any;address.sin6_port=htons(port);
    if(bind(listener,reinterpret_cast<sockaddr*>(&address),sizeof(address))==SOCKET_ERROR||listen(listener,SOMAXCONN)==SOCKET_ERROR){error=SocketError("bind/listen");closesocket(listener);return false;}
    sockaddr_in6 actual{};int size=sizeof(actual);getsockname(listener,reinterpret_cast<sockaddr*>(&actual),&size);port_=ntohs(actual.sin6_port);listen_socket_=FromSocket(listener);running_=true;accept_thread_=std::thread(&SkeletonServer::AcceptLoop,this);error.clear();return true;
}
void SkeletonServer::Stop(){ if(!running_.exchange(false)&&listen_socket_==InvalidSocketStorage)return;CloseSocket(listen_socket_);if(accept_thread_.joinable())accept_thread_.join();std::vector<std::shared_ptr<Peer>> peers;{std::lock_guard lock(mutex_);peers=peers_;}for(auto&p:peers){p->active=false;CloseSocket(p->socket);}for(auto&p:peers)if(p->thread.joinable())p->thread.join();{std::lock_guard lock(mutex_);peers_.clear();frames_.clear();}port_=0; }
bool SkeletonServer::IsRunning()const noexcept{return running_;} std::uint16_t SkeletonServer::Port()const noexcept{return port_;}
std::size_t SkeletonServer::ClientCount()const{std::lock_guard lock(mutex_);return static_cast<std::size_t>(std::count_if(peers_.begin(),peers_.end(),[](const auto&p){return p->active.load();}));}
std::vector<SkeletonFrame> SkeletonServer::Frames()const{std::lock_guard lock(mutex_);std::vector<SkeletonFrame> out;for(const auto&i:frames_)out.push_back(i.second);return out;}
void SkeletonServer::AcceptLoop(){while(running_){SOCKET accepted=accept(ToSocket(listen_socket_),nullptr,nullptr);if(accepted==INVALID_SOCKET)break;auto peer=std::make_shared<Peer>();peer->socket=FromSocket(accepted);std::vector<SkeletonFrame> initial;{std::lock_guard lock(mutex_);peers_.push_back(peer);for(const auto&i:frames_)initial.push_back(i.second);}for(const auto&frame:initial){WireFrame wire=Encode(frame);std::lock_guard lock(peer->send_mutex);if(!SendAll(accepted,reinterpret_cast<const char*>(&wire),sizeof(wire))){peer->active=false;break;}}peer->thread=std::thread(&SkeletonServer::ReceiveLoop,this,peer);}}
void SkeletonServer::ReceiveLoop(const std::shared_ptr<Peer>&peer){while(running_&&peer->active){WireFrame wire{};if(!ReceiveAll(ToSocket(peer->socket),reinterpret_cast<char*>(&wire),sizeof(wire)))break;SkeletonFrame frame;if(!Decode(wire,frame))break;std::string ignored;Broadcast(frame,ignored);}peer->active=false;CloseSocket(peer->socket);}
bool SkeletonServer::Broadcast(const SkeletonFrame&frame,std::string&error){if(frame.source_id.empty()){error="Skeleton source id is required.";return false;}WireFrame wire=Encode(frame);std::vector<std::shared_ptr<Peer>> peers;{std::lock_guard lock(mutex_);frames_[frame.source_id]=frame;peers=peers_;}bool ok=true;for(auto&p:peers)if(p->active){std::lock_guard lock(p->send_mutex);if(!SendAll(ToSocket(p->socket),reinterpret_cast<const char*>(&wire),sizeof(wire))){p->active=false;ok=false;}}if(!ok)error="One or more clients disconnected during broadcast.";else error.clear();return true;}
bool SkeletonServer::Publish(const SkeletonFrame&frame,std::string&error){if(!running_){error="Server is not running.";return false;}return Broadcast(frame,error);}

SkeletonClient::SkeletonClient()=default;SkeletonClient::~SkeletonClient(){Disconnect();}
bool SkeletonClient::Connect(const std::string&url,std::string&error){Disconnect();Endpoint endpoint;if(!ParseEndpoint(url,endpoint,error))return false;if(!GlobalWinsock().ok){error="WSAStartup failed.";return false;}addrinfo hints{};hints.ai_family=AF_UNSPEC;hints.ai_socktype=SOCK_STREAM;hints.ai_protocol=IPPROTO_TCP;addrinfo* addresses=nullptr;auto port=std::to_string(endpoint.port);int result=getaddrinfo(endpoint.host.c_str(),port.c_str(),&hints,&addresses);if(result!=0){error="Could not resolve backend host ("+std::to_string(result)+").";return false;}SOCKET connected=INVALID_SOCKET;for(auto*a=addresses;a;a=a->ai_next){SOCKET candidate=socket(a->ai_family,a->ai_socktype,a->ai_protocol);if(candidate!=INVALID_SOCKET&&connect(candidate,a->ai_addr,static_cast<int>(a->ai_addrlen))==0){connected=candidate;break;}if(candidate!=INVALID_SOCKET)closesocket(candidate);}freeaddrinfo(addresses);if(connected==INVALID_SOCKET){error=SocketError("connect");return false;}socket_=FromSocket(connected);connected_=true;{std::lock_guard lock(mutex_);frames_.clear();last_error_.clear();}receive_thread_=std::thread(&SkeletonClient::ReceiveLoop,this);error.clear();return true;}
void SkeletonClient::Disconnect(){connected_=false;CloseSocket(socket_);if(receive_thread_.joinable()&&receive_thread_.get_id()!=std::this_thread::get_id())receive_thread_.join();}
bool SkeletonClient::IsConnected()const noexcept{return connected_;}
bool SkeletonClient::Publish(const SkeletonFrame&frame,std::string&error){if(!connected_){error="Client is not connected.";return false;}if(frame.source_id.empty()){error="Skeleton source id is required.";return false;}WireFrame wire=Encode(frame);std::lock_guard lock(send_mutex_);if(!SendAll(ToSocket(socket_),reinterpret_cast<const char*>(&wire),sizeof(wire))){error=SocketError("send");connected_=false;CloseSocket(socket_);return false;}error.clear();return true;}
std::vector<SkeletonFrame> SkeletonClient::Frames()const{std::lock_guard lock(mutex_);std::vector<SkeletonFrame> out;for(const auto&i:frames_)out.push_back(i.second);return out;}std::string SkeletonClient::LastError()const{std::lock_guard lock(mutex_);return last_error_;}
void SkeletonClient::ReceiveLoop(){while(connected_){WireFrame wire{};if(!ReceiveAll(ToSocket(socket_),reinterpret_cast<char*>(&wire),sizeof(wire)))break;SkeletonFrame frame;if(!Decode(wire,frame)){std::lock_guard lock(mutex_);last_error_="Server sent an incompatible skeleton packet.";break;}std::lock_guard lock(mutex_);auto found=frames_.find(frame.source_id);if(found==frames_.end()||frame.sequence>=found->second.sequence)frames_[frame.source_id]=std::move(frame);}connected_=false;CloseSocket(socket_);}
} // namespace skeletonnet
