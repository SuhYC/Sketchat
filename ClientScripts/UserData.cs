using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class UserData
{
    private string _nickname;

    // only for init text
    private string _roomName;
    private string _roompw;

    private static UserData _instance;
    public static UserData Instance
    {
        get
        {
            if (_instance == null)
            {
                _instance = new UserData();
            }
            return _instance;
        } 
    }

    public void SetName(string nickname) { _nickname = nickname; return; }
    public void SetRoomName(string roomName_) { _roomName = roomName_; return; }
    public void SetRoomPW(string pw_) { _roompw = pw_; return; }

    public string GetName() { return _nickname; }
    public string GetRoomName() { return _roomName; }
    public string GetRoomPW() { return _roompw; }
}
