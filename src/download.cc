#include "pch.h"
#include "download.h"

#include "common.h"
#include "get_db_handle.h"

#include "wechat_data.h"
#include "base64.h"

#define WX_NEW_CHAT_MSG_OFFSET 0x76f010
#define WX_GET_PRE_DOWNLOAD_MGR_OFFSET 0x80f110
#define WX_PUSH_ATTACH_TASK_OFFSET 0x82bb40
#define WX_FREE_CHAT_MSG_INSTANCE_COUNTER_OFFSET 0x756e30
#define WX_FREE_CHAT_MSG_OFFSET 0x756960
#define WX_CHAT_MGR_OFFSET 0x792700
#define WX_GET_MGR_BY_PREFIX_LOCAL_ID_OFFSET 0xbc0370
#define WX_GET_CURRENT_DATA_PATH_OFFSET 0xc872c0
#define WX_APP_MSG_INFO_OFFSET 0x7b3d20
#define WX_GET_APP_MSG_XML_OFFSET 0xe628a0
#define WX_FREE_APP_MSG_INFO_OFFSET 0x79d900
#define WX_PUSH_THUMB_TASK_OFFSET 0x82ba40
#define WX_VIDEO_MGR_OFFSET 0x829820
#define WX_DOWNLOAD_VIDEO_IMG_OFFSET 0xd46c30

using namespace std;

int DoDownloadTask(ULONG64 msg_id) {
  int success = -1;
  int db_index = 0;
  int local_id = GetLocalIdByMsgId(msg_id, db_index);
  if (local_id < 1) {
    return -2;
  }

  char chat_msg[0x2D8] = {0};
  DWORD base = GetWeChatWinBase();
  DWORD new_chat_msg_addr = base + WX_NEW_CHAT_MSG_OFFSET;
  DWORD get_chat_mgr_addr = base + WX_CHAT_MGR_OFFSET;
  DWORD pre_download_mgr_addr = base + WX_GET_PRE_DOWNLOAD_MGR_OFFSET;
  DWORD push_attach_task_addr = base + WX_PUSH_ATTACH_TASK_OFFSET;
  DWORD free_addr = base + WX_FREE_CHAT_MSG_INSTANCE_COUNTER_OFFSET;
  DWORD get_by_local_Id_addr = base + WX_GET_MGR_BY_PREFIX_LOCAL_ID_OFFSET;
  DWORD get_current_data_path_addr = base + WX_GET_CURRENT_DATA_PATH_OFFSET;
  DWORD free_app_msg_info_addr = base + WX_FREE_APP_MSG_INFO_OFFSET;
  DWORD push_thumb_task_addr = base + WX_PUSH_THUMB_TASK_OFFSET;
  DWORD video_mgr_addr = base + WX_VIDEO_MGR_OFFSET;
  DWORD download_video_image_addr = base + WX_VIDEO_MGR_OFFSET;

  WeChatString current_data_path;

  __asm {
    PUSHAD
    PUSHFD
    LEA        ECX,current_data_path
    CALL       get_current_data_path_addr

    LEA        ECX,chat_msg
    CALL       new_chat_msg_addr

    CALL       get_chat_mgr_addr                                       
    PUSH       dword ptr [db_index]
    LEA        ECX,chat_msg
    PUSH       dword ptr [local_id]
    CALL       get_by_local_Id_addr               
    ADD        ESP,0x8
    POPFD
    POPAD
  }
  wstring save_path = L"";
  wstring thumb_path = L"";
  if (current_data_path.length > 0) {
    save_path += current_data_path.ptr;
    save_path += L"wxhelper";
  } else {
    return -1;
  }
 
  if (!FindOrCreateDirectoryW(save_path.c_str())) {
    return -3;
  }
  DWORD type = *(DWORD *)(chat_msg + 0x38);
  wchar_t *content = *(wchar_t **)(chat_msg + 0x70);

  switch (type) {
    case 0x3: {
      save_path += L"\\image";
      if (!FindOrCreateDirectoryW(save_path.c_str())) {
        return -3;
      }
      save_path = save_path +L"\\"+ to_wstring(msg_id) + L".png";
      break;
    }
    case 0x3E:
    case 0x2B: {
      save_path += L"\\video";
      if (!FindOrCreateDirectoryW(save_path.c_str())) {
        return -3;
      }
      thumb_path = save_path + L"\\"+ to_wstring(msg_id) + L".jpg";
      save_path =  save_path + L"\\"+ to_wstring(msg_id) + L".mp4";
     
      break;
    }
    case 0x31: {
      save_path += L"\\file";
      wcout << save_path << endl;
      if (!FindOrCreateDirectoryW(save_path.c_str())) {
        return -3;
      }
      char xml_app_msg[0xC80] = {0};
      DWORD new_app_msg_addr = base + WX_APP_MSG_INFO_OFFSET;
      DWORD get_xml_addr = base + WX_GET_APP_MSG_XML_OFFSET;
      WeChatString w_content(content);

      __asm {
        PUSHAD
        PUSHFD
        LEA        ECX,xml_app_msg      
        CALL       new_app_msg_addr 
        PUSH       0x1
        LEA        EAX,w_content
        PUSH       EAX       
        LEA        ECX,xml_app_msg
        CALL       get_xml_addr 
        MOV        success,EAX
        LEA        ECX,xml_app_msg
        CALL       free_app_msg_info_addr
        POPFD
        POPAD
      }
      if (success != 1) {
        return -4;
      }
      WeChatString *file_name = (WeChatString *)((DWORD)xml_app_msg + 0x44);
      save_path = save_path +L"\\" + to_wstring(msg_id) + L"_" +
                  wstring(file_name->ptr, file_name->length);
      break;
    }
    default:
      break;
  }
  WeChatString  w_save_path(save_path);
  WeChatString  w_thumb_path(thumb_path);
  int temp =1;
  memcpy(&chat_msg[0x19C], &w_thumb_path, sizeof(w_thumb_path));
  memcpy(&chat_msg[0x1B0], &w_save_path, sizeof(w_save_path));
  memcpy(&chat_msg[0x29C], &temp, sizeof(temp));
  // note： the image has been downloaded and will not be downloaded again
  // use low-level method  
  // this function does not work, need to modify chatmsg.
  // if (type == 0x3E || type == 0x2B){
  //   __asm{
  //      PUSHAD
  //      PUSHFD
  //      CALL       video_mgr_addr
  //      LEA        ECX,chat_msg
  //      PUSH       ECX
  //      MOV        ECX,EAX
  //      CALL       download_video_image_addr
  //      POPFD
  //      POPAD
  //   }
  // }

  __asm {
    PUSHAD
    PUSHFD
    CALL       pre_download_mgr_addr                                
    PUSH       0x1
    PUSH       0x0
    LEA        ECX,chat_msg
    PUSH       ECX
    MOV        ECX,EAX
    CALL       push_attach_task_addr
    MOV        success,EAX
    LEA        ECX,chat_msg
    PUSH       0x0 
    CALL       free_addr
    POPFD
    POPAD
  }

  return success;
}

int GetVoice(ULONG64 msg_id, wchar_t *dir) {
  int success = -1;
  string buff = GetVoiceBuffByMsgId(msg_id);
  if (buff.size() == 0) {
    success = 0;
    return success;
  }
  wstring save_path = wstring(dir);
  if (!FindOrCreateDirectoryW(save_path.c_str())) {
    success = -2;
    return success;
  }
  save_path = save_path + L"\\" + to_wstring(msg_id) + L".amr";
  HANDLE file_handle = CreateFileW(save_path.c_str(), GENERIC_ALL, 0, NULL,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file_handle == INVALID_HANDLE_VALUE) {
    #ifdef _DEBUG
    wcout <<" save_path =" <<save_path<<endl;
    #endif
    return success;
  }
  DWORD bytes_write = 0;
  string decode = base64_decode(buff);
  WriteFile(file_handle, (LPCVOID)decode.c_str(), decode.size(), &bytes_write, 0);
  CloseHandle(file_handle);
  success = 1;
  return success;
}