#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define PORT 8080
#define BUF_SIZE 16384
#define DB_FILE "smartpark.db"
static char active_session[65]="", csrf_token[65]="";
static const char *ADMIN_USER="admin", *ADMIN_PASS="admin123";
static void get_json_value(const char *body,const char *key,char *out,size_t cap){
 char needle[100];snprintf(needle,sizeof needle,"\"%s\"",key);const char*p=strstr(body,needle);out[0]=0;if(!p)return;p=strchr(p,':');if(!p)return;p++;while(*p==' '||*p=='\t')p++;if(*p=='"'){p++;size_t n=0;while(*p&&*p!='"'&&n+1<cap){if(*p=='\\'&&p[1])p++;out[n++]=*p++;}out[n]=0;}
}
static void send_response(SOCKET s,int status,const char*type,const char*body,const char*extra){
 const char*reason=status==200?"OK":status==400?"Bad Request":status==401?"Unauthorized":status==403?"Forbidden":status==404?"Not Found":status==409?"Conflict":"Internal Server Error";
 char h[2048];int n=(int)strlen(body);int z=snprintf(h,sizeof h,"HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nCache-Control: no-store\r\nConnection: close\r\n%s\r\n",status,reason,type,n,extra?extra:"");send(s,h,z,0);send(s,body,n,0);
}
static void send_json(SOCKET s,int status,const char*j,const char*x){send_response(s,status,"application/json; charset=utf-8",j,x);}
static int db_exec(sqlite3*d,const char*q){return sqlite3_exec(d,q,NULL,NULL,NULL)==SQLITE_OK;}
static int init_db(void){
 sqlite3*d=NULL;if(sqlite3_open(DB_FILE,&d)!=SQLITE_OK)return 0;
 const char*q="PRAGMA journal_mode=WAL;CREATE TABLE IF NOT EXISTS parking_slots(id INTEGER PRIMARY KEY,slot_number TEXT UNIQUE,status TEXT NOT NULL DEFAULT 'available',vehicle_number TEXT,booking_time TEXT);CREATE TABLE IF NOT EXISTS parking_history(id INTEGER PRIMARY KEY AUTOINCREMENT,slot_number TEXT NOT NULL,vehicle_number TEXT NOT NULL,booking_time TEXT NOT NULL,release_time TEXT,status TEXT NOT NULL DEFAULT 'active');";
 int ok=db_exec(d,q);sqlite3_stmt*st=NULL;
 if(ok&&sqlite3_prepare_v2(d,"INSERT OR IGNORE INTO parking_slots(slot_number,status) VALUES(?,'available')",-1,&st,NULL)==SQLITE_OK){for(int i=1;i<=10;i++){char v[8];sprintf(v,"P%02d",i);sqlite3_bind_text(st,1,v,-1,SQLITE_TRANSIENT);sqlite3_step(st);sqlite3_reset(st);}sqlite3_finalize(st);}
 sqlite3_close(d);return ok;
}
static int is_admin(const char*r){
 const char*p=strstr(r,"\r\nCookie:");if(!p)return 0;p+=9;while(*p==' ')p++;char expected[100];snprintf(expected,sizeof expected,"sid=%s",active_session);return active_session[0]&&strncmp(p,expected,strlen(expected))==0;
}
static int csrf_ok(const char*r){const char*p=strstr(r,"\r\nX-CSRF-Token:");if(!p||!csrf_token[0])return 0;p+=15;while(*p==' ')p++;return strncmp(p,csrf_token,strlen(csrf_token))==0;}
static void make_token(char*out){unsigned char b[32];if(BCryptGenRandom(NULL,b,32,BCRYPT_USE_SYSTEM_PREFERRED_RNG)!=0){srand((unsigned)time(NULL));for(int i=0;i<32;i++)b[i]=(unsigned char)(rand()%256);}for(int i=0;i<32;i++)sprintf(out+i*2,"%02x",b[i]);out[64]=0;}
static void api_slots(SOCKET s){
 sqlite3*d=NULL;sqlite3_stmt*st=NULL;sqlite3_open(DB_FILE,&d);char out[8192]="{\"success\":true,\"slots\":[",row[400];int first=1,total=0,occ=0;
 if(sqlite3_prepare_v2(d,"SELECT slot_number,status,COALESCE(vehicle_number,''),COALESCE(booking_time,'') FROM parking_slots ORDER BY slot_number",-1,&st,NULL)==SQLITE_OK){while(sqlite3_step(st)==SQLITE_ROW){const char*a=(const char*)sqlite3_column_text(st,0),*b=(const char*)sqlite3_column_text(st,1),*c=(const char*)sqlite3_column_text(st,2),*e=(const char*)sqlite3_column_text(st,3);snprintf(row,sizeof row,"%s{\"slot_number\":\"%s\",\"status\":\"%s\",\"vehicle_number\":\"%s\",\"booking_time\":\"%s\"}",first?"":",",a,b,c,e);if(strlen(out)+strlen(row)<sizeof(out)-100)strcat(out,row);first=0;total++;if(!strcmp(b,"occupied"))occ++;}sqlite3_finalize(st);}
 sqlite3_close(d);char tail[160];snprintf(tail,sizeof tail,"],\"stats\":{\"total\":%d,\"available\":%d,\"occupied\":%d,\"percentage\":%.1f}}",total,total-occ,occ,total?100.0*occ/total:0);strcat(out,tail);send_json(s,200,out,NULL);
}
static void api_history(SOCKET s){
 sqlite3*d=NULL;sqlite3_stmt*st=NULL;sqlite3_open(DB_FILE,&d);char out[10000]="{\"success\":true,\"history\":[",row[600];int first=1;
 if(sqlite3_prepare_v2(d,"SELECT slot_number,vehicle_number,booking_time,COALESCE(release_time,''),status FROM parking_history ORDER BY id DESC LIMIT 100",-1,&st,NULL)==SQLITE_OK){while(sqlite3_step(st)==SQLITE_ROW){snprintf(row,sizeof row,"%s{\"slot_number\":\"%s\",\"vehicle_number\":\"%s\",\"booking_time\":\"%s\",\"release_time\":\"%s\",\"status\":\"%s\"}",first?"":",",sqlite3_column_text(st,0),sqlite3_column_text(st,1),sqlite3_column_text(st,2),sqlite3_column_text(st,3),sqlite3_column_text(st,4));if(strlen(out)+strlen(row)<sizeof(out)-10)strcat(out,row);first=0;}sqlite3_finalize(st);}sqlite3_close(d);strcat(out,"]}");send_json(s,200,out,NULL);
}
static void api_book(SOCKET s,const char*body){
 char slot[32],veh[32];get_json_value(body,"slot_number",slot,sizeof slot);get_json_value(body,"vehicle_number",veh,sizeof veh);for(size_t i=0;veh[i];i++)if(veh[i]==' ')memmove(veh+i,veh+i+1,strlen(veh+i));
 if(!slot[0]||strlen(veh)<4||strlen(veh)>12){send_json(s,400,"{\"success\":false,\"message\":\"Enter a valid slot and vehicle number.\"}",NULL);return;}
 sqlite3*d=NULL;sqlite3_stmt*st=NULL;sqlite3_open(DB_FILE,&d);sqlite3_prepare_v2(d,"SELECT status FROM parking_slots WHERE slot_number=?",-1,&st,NULL);sqlite3_bind_text(st,1,slot,-1,SQLITE_TRANSIENT);int rc=sqlite3_step(st);char status[20]="";if(rc==SQLITE_ROW)snprintf(status,sizeof status,"%s",sqlite3_column_text(st,0));sqlite3_finalize(st);
 if(rc!=SQLITE_ROW){sqlite3_close(d);send_json(s,404,"{\"success\":false,\"message\":\"Slot not found.\"}",NULL);return;}if(strcmp(status,"available")){sqlite3_close(d);send_json(s,409,"{\"success\":false,\"message\":\"Slot is already occupied.\"}",NULL);return;}
 sqlite3_prepare_v2(d,"SELECT 1 FROM parking_slots WHERE vehicle_number=?",-1,&st,NULL);sqlite3_bind_text(st,1,veh,-1,SQLITE_TRANSIENT);rc=sqlite3_step(st);sqlite3_finalize(st);if(rc==SQLITE_ROW){sqlite3_close(d);send_json(s,409,"{\"success\":false,\"message\":\"Vehicle is already parked.\"}",NULL);return;}
 time_t t=time(NULL);char now[32];strftime(now,sizeof now,"%Y-%m-%d %H:%M:%S",localtime(&t));
 sqlite3_prepare_v2(d,"UPDATE parking_slots SET status='occupied',vehicle_number=?,booking_time=? WHERE slot_number=?",-1,&st,NULL);sqlite3_bind_text(st,1,veh,-1,SQLITE_TRANSIENT);sqlite3_bind_text(st,2,now,-1,SQLITE_TRANSIENT);sqlite3_bind_text(st,3,slot,-1,SQLITE_TRANSIENT);sqlite3_step(st);sqlite3_finalize(st);
 sqlite3_prepare_v2(d,"INSERT INTO parking_history(slot_number,vehicle_number,booking_time,status) VALUES(?,?,?,'active')",-1,&st,NULL);sqlite3_bind_text(st,1,slot,-1,SQLITE_TRANSIENT);sqlite3_bind_text(st,2,veh,-1,SQLITE_TRANSIENT);sqlite3_bind_text(st,3,now,-1,SQLITE_TRANSIENT);sqlite3_step(st);sqlite3_finalize(st);sqlite3_close(d);send_json(s,200,"{\"success\":true,\"message\":\"Parking slot booked.\"}",NULL);
}
static void api_release(SOCKET s,const char*body){
 char slot[32],veh[32]="";get_json_value(body,"slot_number",slot,sizeof slot);sqlite3*d=NULL;sqlite3_stmt*st=NULL;sqlite3_open(DB_FILE,&d);sqlite3_prepare_v2(d,"SELECT COALESCE(vehicle_number,'') FROM parking_slots WHERE slot_number=? AND status='occupied'",-1,&st,NULL);sqlite3_bind_text(st,1,slot,-1,SQLITE_TRANSIENT);int rc=sqlite3_step(st);if(rc==SQLITE_ROW)snprintf(veh,sizeof veh,"%s",sqlite3_column_text(st,0));sqlite3_finalize(st);
 if(rc!=SQLITE_ROW){sqlite3_close(d);send_json(s,409,"{\"success\":false,\"message\":\"Slot not found or already available.\"}",NULL);return;}
 time_t t=time(NULL);char now[32];strftime(now,sizeof now,"%Y-%m-%d %H:%M:%S",localtime(&t));
 sqlite3_prepare_v2(d,"UPDATE parking_slots SET status='available',vehicle_number=NULL,booking_time=NULL WHERE slot_number=?",-1,&st,NULL);sqlite3_bind_text(st,1,slot,-1,SQLITE_TRANSIENT);sqlite3_step(st);sqlite3_finalize(st);
 sqlite3_prepare_v2(d,"UPDATE parking_history SET release_time=?,status='released' WHERE slot_number=? AND vehicle_number=? AND status='active'",-1,&st,NULL);sqlite3_bind_text(st,1,now,-1,SQLITE_TRANSIENT);sqlite3_bind_text(st,2,slot,-1,SQLITE_TRANSIENT);sqlite3_bind_text(st,3,veh,-1,SQLITE_TRANSIENT);sqlite3_step(st);sqlite3_finalize(st);sqlite3_close(d);send_json(s,200,"{\"success\":true,\"message\":\"Parking slot released.\"}",NULL);
}
static void serve_static(SOCKET s,const char*path){
 const char*f=!strcmp(path,"/")?"public/index.html":!strcmp(path,"/style.css")?"public/style.css":!strcmp(path,"/app.js")?"public/app.js":NULL;if(!f){send_json(s,404,"{\"message\":\"Not found\"}",NULL);return;}FILE*fp=fopen(f,"rb");if(!fp){send_json(s,404,"{\"message\":\"Frontend file missing\"}",NULL);return;}fseek(fp,0,SEEK_END);long n=ftell(fp);rewind(fp);char*b=malloc((size_t)n+1);if(!b){fclose(fp);return;}fread(b,1,(size_t)n,fp);b[n]=0;fclose(fp);const char*t=strstr(f,".css")?"text/css":strstr(f,".js")?"application/javascript":"text/html";send_response(s,200,t,b,NULL);free(b);
}
static void handle(SOCKET s){
 char req[BUF_SIZE];int n=recv(s,req,sizeof(req)-1,0);if(n<=0)return;req[n]=0;char method[8]="",path[200]="";sscanf(req,"%7s %199s",method,path);char*body=strstr(req,"\r\n\r\n");body=body?body+4:"";
 if(!strcmp(method,"GET")&&(!strcmp(path,"/")||!strcmp(path,"/style.css")||!strcmp(path,"/app.js"))){serve_static(s,path);return;}
 if(!strcmp(method,"GET")&&!strcmp(path,"/api/slots")){api_slots(s);return;}
 if(!strcmp(method,"GET")&&!strcmp(path,"/api/history")){if(!is_admin(req)){send_json(s,401,"{\"message\":\"Admin login required\"}",NULL);return;}api_history(s);return;}
 if(!strcmp(method,"POST")&&!strcmp(path,"/api/login")){char u[80],p[100];get_json_value(body,"username",u,sizeof u);get_json_value(body,"password",p,sizeof p);if(strcmp(u,ADMIN_USER)||strcmp(p,ADMIN_PASS)){send_json(s,401,"{\"message\":\"Invalid username or password\"}",NULL);return;}make_token(active_session);make_token(csrf_token);char extra[256],json[160];snprintf(extra,sizeof extra,"Set-Cookie: sid=%s; HttpOnly; SameSite=Strict; Path=/\r\n",active_session);snprintf(json,sizeof json,"{\"success\":true,\"csrf_token\":\"%s\"}",csrf_token);send_json(s,200,json,extra);return;}
 if(!strcmp(method,"POST")&&(!strcmp(path,"/api/book")||!strcmp(path,"/api/release")||!strcmp(path,"/api/logout"))){if(!is_admin(req)){send_json(s,401,"{\"message\":\"Admin login required\"}",NULL);return;}if(!csrf_ok(req)){send_json(s,403,"{\"message\":\"Invalid security token. Please log in again.\"}",NULL);return;}if(!strcmp(path,"/api/book"))api_book(s,body);else if(!strcmp(path,"/api/release"))api_release(s,body);else{active_session[0]=csrf_token[0]=0;send_json(s,200,"{\"success\":true}","Set-Cookie: sid=; Max-Age=0; Path=/\r\n");}return;}
 send_json(s,404,"{\"message\":\"Route not found\"}",NULL);
}
int main(void){
 if(!init_db()){puts("SQLite database initialization failed.");return 1;}WSADATA w;if(WSAStartup(MAKEWORD(2,2),&w)){puts("Winsock startup failed.");return 1;}SOCKET server=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);struct sockaddr_in a;memset(&a,0,sizeof a);a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=htons(PORT);if(server==INVALID_SOCKET||bind(server,(struct sockaddr*)&a,sizeof a)==SOCKET_ERROR||listen(server,10)==SOCKET_ERROR){printf("Could not start on port %d. Close any app using this port.\n",PORT);return 1;}printf("SmartPark C server: http://127.0.0.1:%d\nAdmin demo login: admin / admin123\nCtrl+C stops the server.\n",PORT);for(;;){SOCKET c=accept(server,NULL,NULL);if(c!=INVALID_SOCKET){handle(c);closesocket(c);}}return 0;
}