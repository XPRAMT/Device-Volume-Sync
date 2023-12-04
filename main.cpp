#include <windows.h>
#include <shellapi.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <iostream>
using namespace std;

BOOL per_isMuted;
float per_volume;
class MyAudioEndpointVolumeCallback : public IAudioEndpointVolumeCallback {
public:
    // 增加參考計數
    ULONG STDMETHODCALLTYPE AddRef() { return 1; }
    // 釋放參考計數
    ULONG STDMETHODCALLTYPE Release() { return 1; }
    // 查詢介面
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID** ppvInterface) {
        if (riid == IID_IUnknown || riid == __uuidof(IAudioEndpointVolumeCallback)) {
            *ppvInterface = static_cast<IAudioEndpointVolumeCallback*>(this);
            return S_OK;
        }
        else {
            *ppvInterface = NULL;
            return E_NOINTERFACE;
        }
    }

    // 音量通知回呼函式
    HRESULT STDMETHODCALLTYPE OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA pNotify) {
        if (pNotify) {
            // 在此處處理音量變更通知
            float volume = pNotify->fMasterVolume;
            BOOL isMuted = pNotify->bMuted;

            if (volume < 0.01) {
                isMuted = 1;
            }

            if (per_isMuted != isMuted) {
                SyncNonDefaultDevicesToDefault(isMuted, volume,1);
                if (isMuted == 1) {cout << "Muted" << endl;}else{cout << "UnMute" << endl; }
            }
            if (per_volume != volume && volume>=0.01 ) {
                SyncNonDefaultDevicesToDefault(isMuted,volume,2);
                cout << "volume:" << volume * 100 << "%" << endl;
            }
            per_isMuted = isMuted;
            per_volume = volume;
        }
        return S_OK;
    }

 void SyncNonDefaultDevicesToDefault(BOOL isMuted,float volume,int state) {
        HRESULT hr;
        IMMDeviceEnumerator* deviceEnumerator = NULL;
        IMMDevice* defaultDevice = NULL;
        IAudioEndpointVolume* defaultEndpointVolume = NULL;

        // 初始化 COM 库
        hr = CoInitialize(NULL);

        // 創建設備枚舉器
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER,
            __uuidof(IMMDeviceEnumerator), (LPVOID*)&deviceEnumerator);

        IMMDeviceCollection* deviceCollection = NULL;
        // 枚舉音訊端點
        hr = deviceEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &deviceCollection);

        UINT deviceCount = 0;
        hr = deviceCollection->GetCount(&deviceCount);

        for (UINT i = 0; i < deviceCount; i++) {
            IMMDevice* currentDevice = NULL;
            hr = deviceCollection->Item(i, &currentDevice);
            if (currentDevice) {
                IAudioEndpointVolume* currentEndpointVolume = NULL;

                // 啟用當前端點音量界面
                hr = currentDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER,NULL, (LPVOID*)&currentEndpointVolume);

                // 將指定的音量值應用到非默認設備
                if (currentEndpointVolume) {
                    // 將指定的聲音狀態應用到非默認設備
                    switch (state) {
                    case 1:
                        hr = currentEndpointVolume->SetMute(isMuted, NULL);
                        break;
                    case 2:
                        hr = currentEndpointVolume->SetMasterVolumeLevelScalar(volume, NULL);
                        break;
                    }
                    currentEndpointVolume->Release();
                }
            }
            if (currentDevice) currentDevice->Release();
        }

        if (defaultEndpointVolume) defaultEndpointVolume->Release();
        if (defaultDevice) defaultDevice->Release();
        if (deviceCollection) deviceCollection->Release();
        if (deviceEnumerator) deviceEnumerator->Release();

        // 反初始化 COM 库
        CoUninitialize();
    }
};


int main() {

    HRESULT hr;
    IMMDeviceEnumerator* deviceEnumerator = NULL;
    IMMDevice* defaultDevice = NULL;
    IAudioEndpointVolume* defaultEndpointVolume = NULL;
    MyAudioEndpointVolumeCallback* endpointVolumeCallback = new MyAudioEndpointVolumeCallback();

    // 初始化 COM Library
    hr = CoInitialize(NULL);
    // 創建設備枚舉器
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER,
        __uuidof(IMMDeviceEnumerator), (LPVOID*)&deviceEnumerator);

    // 獲取默認音訊端點
     hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice);
    // 啟用默認端點音量界面
    hr = defaultDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER,NULL, (LPVOID*)&defaultEndpointVolume);

    // 註冊音量變更通知的回呼介面
    hr = defaultEndpointVolume->RegisterControlChangeNotify(endpointVolumeCallback);

    //隱藏窗口
    ShowWindow(GetConsoleWindow(), SW_HIDE);

    // 保持執行
    while (true) {
        Sleep(10000); //（根據需要調整延遲時間）
    }

    // 取消註冊預設設備的回呼介面
    hr = defaultEndpointVolume->UnregisterControlChangeNotify(endpointVolumeCallback);

    // 釋放資源
    if (defaultEndpointVolume) defaultEndpointVolume->Release();
    if (defaultDevice) defaultDevice->Release();
    if (deviceEnumerator) deviceEnumerator->Release();
    if (endpointVolumeCallback) delete endpointVolumeCallback;

    // 反初始化 COM Library
    CoUninitialize();

    return 0;
}

