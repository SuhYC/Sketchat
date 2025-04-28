using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class RequestRoomInfoButton : MonoBehaviour
{
    private float lastCallTime = 0.0f;
    private float cooltime = 0.03f;

    public async void OnClick()
    {
        if(lastCallTime + cooltime > Time.time)
        {
            return;
        }

        lastCallTime = Time.time;

        await PacketMaker.Instance.ReqRoomInfo();
    }
}
