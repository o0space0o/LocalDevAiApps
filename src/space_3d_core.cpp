#include "space_3d_core.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace space3d {
namespace {
constexpr float E = 1e-6f;
float clampf(float v,float a,float b){return std::max(a,std::min(b,v));}
bool finite3(const Vector3& v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
}
Vector3 Vector3::operator+(const Vector3& v)const{return{x+v.x,y+v.y,z+v.z};}
Vector3 Vector3::operator-(const Vector3& v)const{return{x-v.x,y-v.y,z-v.z};}
Vector3 Vector3::operator-()const{return{-x,-y,-z};}
Vector3 Vector3::operator*(float s)const{return{x*s,y*s,z*s};}
Vector3 Vector3::operator/(float s)const{return std::abs(s)<E?Vector3{}:Vector3{x/s,y/s,z/s};}
Vector3& Vector3::operator+=(const Vector3& v){x+=v.x;y+=v.y;z+=v.z;return *this;}
Vector3& Vector3::operator-=(const Vector3& v){x-=v.x;y-=v.y;z-=v.z;return *this;}
Vector3& Vector3::operator*=(float s){x*=s;y*=s;z*=s;return *this;}
float Vector3::Dot(const Vector3& v)const{return x*v.x+y*v.y+z*v.z;}
Vector3 Vector3::Cross(const Vector3& v)const{return{y*v.z-z*v.y,z*v.x-x*v.z,x*v.y-y*v.x};}
float Vector3::LengthSquared()const{return Dot(*this);}
float Vector3::Length()const{return std::sqrt(LengthSquared());}
Vector3 Vector3::Normalized()const{float n=Length();return n>E?*this/n:Vector3{};}

Vector3 Camera::Position()const{float c=std::cos(pitch);return target+Vector3{std::sin(yaw)*c,std::sin(pitch),std::cos(yaw)*c}*distance;}
Vector3 Camera::Forward()const{return(target-Position()).Normalized();}
Vector3 Camera::Right()const{Vector3 r=Forward().Cross({0,1,0}).Normalized();return r.LengthSquared()>E?r:Vector3{1,0,0};}
Vector3 Camera::Up()const{return Right().Cross(Forward()).Normalized();}
void Camera::Orbit(float y,float p){yaw+=y;pitch=clampf(pitch+p,-1.48f,1.48f);}
void Camera::Zoom(float amount){distance=clampf(distance+amount,3,100);}
ScreenPoint Camera::Project(const Vector3& w,float width,float height)const{
 ScreenPoint s;if(width<=1||height<=1)return s;Vector3 d=w-Position();s.depth=d.Dot(Forward());if(s.depth<=near_plane)return s;
 float f=1/std::tan(vertical_fov_radians*.5f),nx=d.Dot(Right())*f/(s.depth*(width/height)),ny=d.Dot(Up())*f/s.depth;
 s.x=(nx*.5f+.5f)*width;s.y=(.5f-ny*.5f)*height;s.visible=std::abs(nx)<=1.2f&&std::abs(ny)<=1.2f;return s;
}
Ray Camera::MakeRay(float x,float y,float width,float height)const{
 Ray r{Position(),Forward()};if(width<=1||height<=1)return r;float t=std::tan(vertical_fov_radians*.5f);
 float nx=(2*x/width-1)*(width/height)*t,ny=(1-2*y/height)*t;r.direction=(Forward()+Right()*nx+Up()*ny).Normalized();return r;
}
float Ball::Volume()const{return 4.0f/3.0f*Pi*radius*radius*radius;}
float Ball::Density()const{return Volume()>E?mass/Volume():0;}
float Ball::KineticEnergy()const{return .5f*mass*velocity.LengthSquared();}

Space3DEngine::Space3DEngine(){Reset(Preset::CollisionLab);}
std::uint32_t Space3DEngine::AddBall(const Vector3&p,const Vector3&v,float r,float m){
 if(!finite3(p)||!finite3(v)||!std::isfinite(r)||!std::isfinite(m)||r<=0||m<=0)return 0;
 Ball b;b.id=next_id_++;b.position=p;b.velocity=v;b.radius=clampf(r,.05f,5);b.mass=clampf(m,.01f,10000);balls_.push_back(b);return b.id;
}
bool Space3DEngine::RemoveBall(std::uint32_t id){auto i=std::find_if(balls_.begin(),balls_.end(),[&](const Ball&b){return b.id==id;});if(i==balls_.end())return false;balls_.erase(i);return true;}
Ball* Space3DEngine::FindBall(std::uint32_t id){auto i=std::find_if(balls_.begin(),balls_.end(),[&](const Ball&b){return b.id==id;});return i==balls_.end()?nullptr:&*i;}
const Ball* Space3DEngine::FindBall(std::uint32_t id)const{auto i=std::find_if(balls_.begin(),balls_.end(),[&](const Ball&b){return b.id==id;});return i==balls_.end()?nullptr:&*i;}
bool Space3DEngine::ApplyImpulse(std::uint32_t id,const Vector3&i){Ball*b=FindBall(id);if(!b||!finite3(i))return false;b->velocity+=i/b->mass;return true;}
void Space3DEngine::Update(float elapsed){if(paused_||!std::isfinite(elapsed)||elapsed<=0)return;accumulator_+=std::min(elapsed,.25f);float s=clampf(settings_.fixed_step,.001f,.1f);int guard=0;while(accumulator_>=s&&guard++<250){SimulateStep(s);accumulator_-=s;}}
void Space3DEngine::StepOnce(){SimulateStep(clampf(settings_.fixed_step,.001f,.1f));}
void Space3DEngine::Clear(){balls_.clear();accumulator_=0;}
void Space3DEngine::Reset(Preset p){
 Clear();simulation_steps_=collision_count_=0;simulated_seconds_=0;next_id_=1;paused_=false;
 if(p==Preset::CollisionLab){AddBall({-6,0,0},{7,0,0},1,2);AddBall({6,0,0},{-7,0,0},1,2);AddBall({0,5,2},{0,0,-2},.7f,1);}
 else if(p==Preset::DropTower)for(int i=0;i<8;++i)AddBall({0,settings_.ground_y+1+i*2.05f,0},{},.9f,1+i*.2f);
 else if(p==Preset::Fountain)for(int i=0;i<12;++i){float a=2*Pi*i/12;AddBall({0,settings_.ground_y+1,0},{std::cos(a)*3.5f,11.0f+(i%3),std::sin(a)*3.5f},.35f,.5f);}
}
void Space3DEngine::SetPaused(bool v){paused_=v;}bool Space3DEngine::IsPaused()const{return paused_;}
void Space3DEngine::SimulateStep(float dt){float drag=std::exp(-std::max(0.0f,settings_.linear_drag)*dt);for(auto&b:balls_){b.velocity+=settings_.gravity*dt;b.velocity*=drag;b.position+=b.velocity*dt;ResolveWorldCollision(b);}if(settings_.collisions_enabled)ResolveBallCollisions();++simulation_steps_;simulated_seconds_+=dt;}
void Space3DEngine::ResolveWorldCollision(Ball&b){
 float bounce=clampf(settings_.restitution,0,1.2f),extent=std::max(1.0f,settings_.half_extent);
 if(b.position.y-b.radius<settings_.ground_y){b.position.y=settings_.ground_y+b.radius;if(b.velocity.y<0)b.velocity.y=-b.velocity.y*bounce;b.velocity.x*=clampf(settings_.ground_friction,0,1);b.velocity.z*=clampf(settings_.ground_friction,0,1);++collision_count_;}
 auto wall=[&](float&p,float&v){float l=extent-b.radius;if(p>l){p=l;if(v>0)v=-v*bounce;++collision_count_;}else if(p<-l){p=-l;if(v<0)v=-v*bounce;++collision_count_;}};wall(b.position.x,b.velocity.x);wall(b.position.z,b.velocity.z);
}
void Space3DEngine::ResolveBallCollisions(){for(size_t i=0;i<balls_.size();++i)for(size_t j=i+1;j<balls_.size();++j){
 Ball&a=balls_[i];Ball&b=balls_[j];Vector3 d=b.position-a.position;float n=d.Length(),min=a.radius+b.radius;if(n>=min)continue;Vector3 normal=n>E?d/n:Vector3{1,0,0};float ia=1/a.mass,ib=1/b.mass,pen=min-n;a.position-=normal*(pen*ia/(ia+ib));b.position+=normal*(pen*ib/(ia+ib));float closing=(b.velocity-a.velocity).Dot(normal);if(closing<0){float q=-(1+clampf(settings_.restitution,0,1.2f))*closing/(ia+ib);a.velocity-=normal*(q*ia);b.velocity+=normal*(q*ib);}++collision_count_;}}
RayHit Space3DEngine::Raycast(const Ray&r)const{RayHit hit;float best=std::numeric_limits<float>::max();Vector3 dir=r.direction.Normalized();if(dir.LengthSquared()<E)return hit;for(const auto&b:balls_){Vector3 o=r.origin-b.position;float q=o.Dot(dir),disc=q*q-(o.LengthSquared()-b.radius*b.radius);if(disc<0)continue;float d=-q-std::sqrt(disc);if(d<0)d=-q+std::sqrt(disc);if(d>=0&&d<best){best=d;hit.ball_id=b.id;hit.distance=d;hit.point=r.origin+dir*d;}}return hit;}
Diagnostics Space3DEngine::GetDiagnostics()const{Diagnostics d;d.ball_count=balls_.size();d.simulation_steps=simulation_steps_;d.collision_count=collision_count_;d.simulated_seconds=simulated_seconds_;float mass=0;for(const auto&b:balls_){d.total_kinetic_energy+=b.KineticEnergy();d.center_of_mass+=b.position*b.mass;mass+=b.mass;}if(mass>E)d.center_of_mass=d.center_of_mass/mass;return d;}
std::wstring Space3DEngine::GetBallStats(std::uint32_t id)const{const Ball*b=FindBall(id);if(!b)return L"No ball selected";std::wostringstream s;s<<std::fixed<<std::setprecision(2)<<L"Ball #"<<b->id<<L"  position ("<<b->position.x<<L", "<<b->position.y<<L", "<<b->position.z<<L")\r\nSpeed "<<b->velocity.Length()<<L"  mass "<<b->mass<<L"  density "<<b->Density();return s.str();}
std::string Space3DEngine::Serialize()const{
 std::ostringstream s;s<<std::setprecision(9)<<"SPACE3D 1\n"<<settings_.gravity.x<<' '<<settings_.gravity.y<<' '<<settings_.gravity.z<<' '<<settings_.restitution<<' '<<settings_.linear_drag<<' '<<settings_.ground_friction<<' '<<settings_.ground_y<<' '<<settings_.half_extent<<' '<<settings_.fixed_step<<' '<<settings_.collisions_enabled<<"\n"<<camera_.target.x<<' '<<camera_.target.y<<' '<<camera_.target.z<<' '<<camera_.yaw<<' '<<camera_.pitch<<' '<<camera_.distance<<"\n"<<balls_.size()<<"\n";for(const auto&b:balls_)s<<b.id<<' '<<b.position.x<<' '<<b.position.y<<' '<<b.position.z<<' '<<b.velocity.x<<' '<<b.velocity.y<<' '<<b.velocity.z<<' '<<b.radius<<' '<<b.mass<<"\n";return s.str();
}
bool Space3DEngine::Deserialize(const std::string&text,std::string&error){
 std::istringstream s(text);std::string tag;int version=0,collisions=0;s>>tag>>version;if(tag!="SPACE3D"||version!=1){error="Unsupported SPACE3D document.";return false;}SimulationSettings st;Camera cam;size_t count=0;
 if(!(s>>st.gravity.x>>st.gravity.y>>st.gravity.z>>st.restitution>>st.linear_drag>>st.ground_friction>>st.ground_y>>st.half_extent>>st.fixed_step>>collisions>>cam.target.x>>cam.target.y>>cam.target.z>>cam.yaw>>cam.pitch>>cam.distance>>count)||count>10000){error="Malformed settings or object count.";return false;}st.collisions_enabled=collisions!=0;std::vector<Ball> parsed;std::uint32_t maxid=0;for(size_t i=0;i<count;++i){Ball b;if(!(s>>b.id>>b.position.x>>b.position.y>>b.position.z>>b.velocity.x>>b.velocity.y>>b.velocity.z>>b.radius>>b.mass)||b.id==0||!finite3(b.position)||!finite3(b.velocity)||b.radius<=0||b.mass<=0){error="Malformed ball record.";return false;}parsed.push_back(b);maxid=std::max(maxid,b.id);}settings_=st;camera_=cam;balls_=std::move(parsed);next_id_=maxid+1;accumulator_=0;error.clear();return true;
}
const std::vector<Ball>& Space3DEngine::GetBalls()const{return balls_;}Camera& Space3DEngine::GetCamera(){return camera_;}const Camera& Space3DEngine::GetCamera()const{return camera_;}SimulationSettings& Space3DEngine::GetSettings(){return settings_;}const SimulationSettings& Space3DEngine::GetSettings()const{return settings_;}
} // namespace space3d
