#include <windows.h>
#include <shellapi.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <iostream>
using namespace std;

BOOL per_isMuted;
BOOL reLoad=1;
float per_volume;
////////////////////////////////////////////////////////
class NotificationClient : public IMMNotificationClient
{
public:
    NotificationClient() : m_cRef(1), m_pEnumerator(nullptr)
    {
        // 初始化目前執行緒的 COM
        HRESULT hr = CoInitialize(NULL);
        if (FAILED(hr))
        {
            std::cout << "無法初始化 COM" << std::endl;
        }
        else
        {
            // 建立裝置列舉器
            hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&m_pEnumerator);
            if (FAILED(hr))
            {
                std::cout << "無法建立裝置列舉器" << std::endl;
                CoUninitialize();
            }
            else
            {
                // 註冊裝置變更通知
                hr = m_pEnumerator->RegisterEndpointNotificationCallback(this);
                if (FAILED(hr))
                {
                    std::cout << "無法註冊裝置變更通知" << std::endl;
                    m_pEnumerator->Release();
                    m_pEnumerator = nullptr;
                    CoUninitialize();
                }
            }
        }
    }

    ~NotificationClient()
    {
        Close();
    }

    STDMETHOD_(ULONG, AddRef)()
    {
        return InterlockedIncrement(&m_cRef);
    }
    STDMETHOD_(ULONG, Release)()
    {
        ULONG ulRef = InterlockedDecrement(&m_cRef);
        if (ulRef == 0)
        {
            delete this;
        }
        return ulRef;
    }
    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObject)
    {
        if (riid == IID_IUnknown || riid == __uuidof(IMMNotificationClient))
        {
            *ppvObject = static_cast<IMMNotificationClient*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    STDMETHOD(OnDefaultDeviceChanged)(EDataFlow flow, ERole role, LPCWSTR pwstrDeviceId) {// IMMNotificationClient 方法
        // 預設音訊裝置已變更。
        reLoad = 0;
        return S_OK;
    }
    STDMETHOD(OnDeviceAdded)(LPCWSTR pwstrDeviceId) { return S_OK; }
    STDMETHOD(OnDeviceRemoved)(LPCWSTR pwstrDeviceId) { return S_OK; }
    STDMETHOD(OnDeviceStateChanged)(LPCWSTR pwstrDeviceId, DWORD dwNewState) { return S_OK; }
    STDMETHOD(OnPropertyValueChanged)(LPCWSTR pwstrDeviceId, const PROPERTYKEY key) { return S_OK; }

    void Close()
    {
        // 取消註冊裝置列舉器
        if (m_pEnumerator)
        {
            m_pEnumerator->UnregisterEndpointNotificationCallback(this);
            m_pEnumerator->Release();
            m_pEnumerator = nullptr;
        }

        // 結束目前執行緒的 COM 函式庫
        CoUninitialize();
    }

private:
    LONG m_cRef;
    IMMDeviceEnumerator* m_pEnumerator;
};
////////////////////////////////////////////////////////
class AudioDeviceNotificationListener
{
public:
    AudioDeviceNotificationListener() : bDidStart(false), pNotificationClient(nullptr), pEnumerator(nullptr), hNotificationThread(nullptr) {}

    ~AudioDeviceNotificationListener() { Close(); }

    bool Start()
    {
        if (!bDidStart)
        {
            HRESULT hr = CoInitialize(NULL);
            if (FAILED(hr))
            {
                return false;
            }

            hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&pEnumerator));
            if (FAILED(hr))
            {
                CleanUp();
                return false;
            }

            pNotificationClient = new NotificationClient();

            hr = pEnumerator->RegisterEndpointNotificationCallback(pNotificationClient);
            if (FAILED(hr))
            {
                CleanUp();
                return false;
            }

            hNotificationThread = CreateThread(NULL, 0, NotificationThreadProc, pNotificationClient, 0, NULL);
            if (hNotificationThread == NULL)
            {
                CleanUp();
                return false;
            }

            bDidStart = true;
            return true;
        }

        return false;
    }

    void Close()
    {
        if (bDidStart)
        {
            CleanUp();
            bDidStart = false;
        }
    }

private:
    // 靜態成員函式：處理通知執行緒
    static DWORD WINAPI NotificationThreadProc(LPVOID lpParameter)
    {
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        return 0;
    }

    // 清理函式：釋放資源和取消註冊通知
    void CleanUp()
    {
        if (hNotificationThread)
        {
            // 發送終止訊息至通知執行緒並等待其結束
            PostThreadMessage(GetThreadId(hNotificationThread), WM_QUIT, NULL, NULL);
            WaitForSingleObject(hNotificationThread, INFINITE);
            CloseHandle(hNotificationThread);
            hNotificationThread = nullptr;
        }

        if (pEnumerator)
        {
            // 取消註冊通知回調
            pEnumerator->UnregisterEndpointNotificationCallback(pNotificationClient);
            pEnumerator->Release();
            pEnumerator = nullptr;
        }

        if (pNotificationClient)
        {
            // 釋放通知客戶端
            pNotificationClient->Release();
            pNotificationClient = nullptr;
        }

        // 取消 COM 初始化
        CoUninitialize();
    }

    bool bDidStart; // 標記是否已開始監聽
    NotificationClient* pNotificationClient; // 通知客戶端
    IMMDeviceEnumerator* pEnumerator; // 裝置列舉器
    HANDLE hNotificationThread; // 通知執行緒的處理
};
////////////////////////////////////////////////////////
class AudioEndpointVolumeCallback : public IAudioEndpointVolumeCallback
{
public:
    // 增加參考計數
    ULONG STDMETHODCALLTYPE AddRef() { return 1; }
    // 釋放參考計數
    ULONG STDMETHODCALLTYPE Release() { return 1; }
    // 查詢介面
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID **ppvInterface)
    {
        if (riid == IID_IUnknown || riid == __uuidof(IAudioEndpointVolumeCallback))
        {
            *ppvInterface = static_cast<IAudioEndpointVolumeCallback *>(this);
            return S_OK;
        }
        else
        {
            *ppvInterface = NULL;
            return E_NOINTERFACE;
        }
    }

    // 音量通知回呼函式
    HRESULT STDMETHODCALLTYPE OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA pNotify)
    {
        if (pNotify)
        {
            // 在此處處理音量變更通知
            float volume = pNotify->fMasterVolume;
            BOOL isMuted = pNotify->bMuted;
            //如果音量小於1%則設為靜音
            if (volume < 0.01)
            {
                isMuted = 1;
            }
            //取到小數點後兩位
            volume = roundf(volume * 100) / 100;
            //如果靜音狀態改變
            if (per_isMuted != isMuted)
            {
                SyncNonDefaultDevicesToDefault(isMuted, volume, 1);
                if (isMuted == 1)
                {
                    cout  << "Muted" << endl;
                }
                else
                {
                    cout << "UnMute" << endl;
                }
            }
            //如果音量狀態改變
            if (per_volume != volume && volume >= 0.01)
            {
                SyncNonDefaultDevicesToDefault(isMuted, volume, 2);
                cout << "volume:" << volume * 100 << "%" << endl;
            }
            per_isMuted = isMuted;
            per_volume = volume;
        }
        return S_OK;
    }

    void SyncNonDefaultDevicesToDefault(BOOL isMuted, float volume, int state)
    {
        HRESULT hr;
        IMMDeviceEnumerator *deviceEnumerator = NULL;
        IMMDevice *defaultDevice = NULL;
        IAudioEndpointVolume *defaultEndpointVolume = NULL;

        // 初始化 COM 库
        hr = CoInitialize(NULL);

        // 創建設備枚舉器
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER,
                              __uuidof(IMMDeviceEnumerator), (LPVOID *)&deviceEnumerator);

        IMMDeviceCollection *deviceCollection = NULL;
        // 枚舉音訊端點
        hr = deviceEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &deviceCollection);

        UINT deviceCount = 0;
        hr = deviceCollection->GetCount(&deviceCount);

        for (UINT i = 0; i < deviceCount; i++)
        {
            IMMDevice *currentDevice = NULL;
            hr = deviceCollection->Item(i, &currentDevice);
            if (currentDevice)
            {
                IAudioEndpointVolume *currentEndpointVolume = NULL;

                // 啟用當前端點音量界面
                hr = currentDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, NULL, (LPVOID *)&currentEndpointVolume);

                // 將指定的音量值應用到非默認設備
                if (currentEndpointVolume)
                {
                    // 將指定的聲音狀態應用到非默認設備
                    switch (state)
                    {
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
            if (currentDevice)
                currentDevice->Release();
        }

        if (defaultEndpointVolume)
            defaultEndpointVolume->Release();
        if (defaultDevice)
            defaultDevice->Release();
        if (deviceCollection)
            deviceCollection->Release();
        if (deviceEnumerator)
            deviceEnumerator->Release();

        // 反初始化 COM 库
        CoUninitialize();
    }
};
////////////////////////////////////////////////////////

int main()
{
    // Initialize COM for the main thread
    HRESULT hr = CoInitialize(NULL);

    // Create an instance of AudioDeviceNotificationListener
    AudioDeviceNotificationListener listener;

    // Start the listener
    listener.Start();
    cout << "Start" << endl;
    // 隱藏窗口
    ShowWindow(GetConsoleWindow(), SW_HIDE);
    while (true)
    {
        HRESULT hr;
        IMMDeviceEnumerator *deviceEnumerator = NULL;
        IMMDevice *defaultDevice = NULL;
        IAudioEndpointVolume *defaultEndpointVolume = NULL;
        AudioEndpointVolumeCallback *endpointVolumeCallback = new AudioEndpointVolumeCallback();

        // 初始化 COM Library
        hr = CoInitialize(NULL);
        // 創建設備枚舉器
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER,
                              __uuidof(IMMDeviceEnumerator), (LPVOID *)&deviceEnumerator);

        // 獲取默認音訊端點
        hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice);
        // 啟用默認端點音量界面
        hr = defaultDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, NULL, (LPVOID *)&defaultEndpointVolume);

        // 註冊音量變更通知的回呼介面
        hr = defaultEndpointVolume->RegisterControlChangeNotify(endpointVolumeCallback);

        // 保持執行
        while (reLoad)
        {
            Sleep(500); // （根據需要調整延遲時間）
        }
        cout <<"Default audio device changed" << endl;
        reLoad=1;
        // 取消註冊預設設備的回呼介面
        hr = defaultEndpointVolume->UnregisterControlChangeNotify(endpointVolumeCallback);

        // 釋放資源
        if (defaultEndpointVolume)
            defaultEndpointVolume->Release();
        if (defaultDevice)
            defaultDevice->Release();
        if (deviceEnumerator)
            deviceEnumerator->Release();
        if (endpointVolumeCallback)
            delete endpointVolumeCallback;

        // 反初始化 COM Library
        CoUninitialize();
    }

    // Close the listener and clean up
    listener.Close();
    CoUninitialize();

    return 0;
}
