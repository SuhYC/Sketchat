using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class RoomPanel : MonoBehaviour
{
    private GameObject m_prefab;
    private ushort m_selected_Room_Num;

    private static RoomPanel _instance;

    public static RoomPanel Instance
    {
        get { return _instance; }
    }

    private void Awake()
    {
        _instance = this;

        m_prefab = Resources.Load<GameObject>("Prefabs/RoomButton");

        if(m_prefab == null )
        {
            Debug.Log($"RoomPanel::Awake : prefab null ref.");
        }
    }

    public void Init()
    {
        foreach(Transform child in transform)
        {
            Destroy(child.gameObject);
        }
    }

    public void Add(ushort roomNum, string roomName, ushort nowUser, ushort maxUser)
    {
        GameObject obj = Instantiate(m_prefab);
        obj.transform.SetParent(transform);

        RoomButton script = obj.GetComponent<RoomButton>();

        script.Init(roomNum, roomName, nowUser, maxUser);
    }

    public void SetSelectedRoomNum(ushort roomNum_)
    {
        m_selected_Room_Num = roomNum_;

        GameObject obj = transform.parent.GetChild(2)?.gameObject;

        if(obj != null)
        {
            obj.SetActive(true);
        }
    }

    public ushort GetSelectedRoomNum() { return m_selected_Room_Num; }
}
