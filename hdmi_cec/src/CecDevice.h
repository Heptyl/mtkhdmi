

class CecDevice {
  public:
    CecDevice(unsigned int portId);
    ~CecDevice();
    int init(const char* path);
    void release();

    unsigned int mPortId;
    unsigned int mType;
    bool mIsDpDevice;
    //device node for CEC uevent
    std::string mUeventNode;
    // connect property for CEC uevent.
    std::string mUeventCnnProp;

    std::string mConnectStateNode;

    int mCecFd;
    int mEventThreadExitFd;

    std::string mCapsName;
    std::string mCapsDriver;

};

