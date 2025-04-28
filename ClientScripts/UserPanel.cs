using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class UserPanel : MonoBehaviour
{
    private static UserPanel _instance;
    private Dictionary<ushort, UserNamePanel> _usernum_to_usernamepanel;
    public static UserPanel Instance { get { return _instance; } }

    private void Awake()
    {
        _instance = this;
        _usernum_to_usernamepanel = new Dictionary<ushort, UserNamePanel>();

        Init();
    }

    public void Init()
    {
        foreach(Transform child in transform)
        {
            UserNamePanel namePanel = child.GetComponent<UserNamePanel>();

            if (namePanel != null)
            {
                namePanel.SetActive(false);
            }
            else
            {
                Debug.Log($"UserPanel::Init : namePanel[{child.GetSiblingIndex()}] doesnt exist.");
            }
        }
    }

    public void UserEnter(ushort usernum_, string name_)
    {
        UserNamePanel namePanel = null;

        foreach(Transform child in transform)
        {
            UserNamePanel tmp = child.GetComponent<UserNamePanel>();

            if (!tmp.IsActive())
            {
                namePanel = tmp;
                break;
            }
        }

        if(namePanel == null)
        {
            Debug.Log($"UserPanel::UserEnter : Tried Enter Full Room.");
            return;
        }

        _usernum_to_usernamepanel.Add(usernum_, namePanel);

        namePanel.SetActive(true);
        namePanel.SetName(name_);

        return;
    }

    public void UserExit(ushort usernum_)
    {
        UserNamePanel namePanel = null;

        if(!_usernum_to_usernamepanel.TryGetValue(usernum_, out namePanel))
        {
            Debug.Log($"UserPanel::UserExit : that user doesnt exist.");
            return;
        }

        _usernum_to_usernamepanel.Remove(usernum_);

        namePanel.SetActive(false);
    }

    public void UserChat(ushort usernum_, string text_)
    {
        UserNamePanel userPanel = null;

        if(!_usernum_to_usernamepanel.TryGetValue(usernum_,out userPanel))
        {
            Debug.Log($"UserPanel::UserChat : that user doesnt exist.");
            return;
        }

        userPanel.Chat(text_);
    }
}
