using System.Collections;
using System.Collections.Generic;
using TMPro;
using UnityEngine;
using UnityEngine.UI;

public class RoomButton : MonoBehaviour
{
    private ushort m_roomNum;

    private TextMeshProUGUI nameText;
    private TextMeshProUGUI userText;

    public void Init(ushort roomNum, string roomName, ushort nowUser, ushort maxUser)
    {
        if(nameText == null)
        {
            nameText = transform.GetChild(0)?.GetComponent<TextMeshProUGUI>();
            userText = transform.GetChild(1)?.GetComponent<TextMeshProUGUI>();
        }

        if(nameText == null)
        {
            Debug.Log($"RoomButton::Init : nameText null ref.");
        }
        else
        {
            nameText.text = roomName;
        }

        if(userText == null)
        {
            Debug.Log($"RoomButton::Init : userText null ref.");
        }
        else
        {
            string[] strings = { "(", nowUser.ToString(), "/", maxUser.ToString(), ")" };

            userText.text = string.Join("", strings);
        }

        m_roomNum = roomNum;
    }

    public void OnClick()
    {
        RoomPanel.Instance.SetSelectedRoomNum(m_roomNum);
    }
}
