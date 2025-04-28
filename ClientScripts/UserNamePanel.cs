using System.Collections;
using System.Collections.Generic;
using TMPro;
using UnityEngine;
using UnityEngine.UI;

public class UserNamePanel : MonoBehaviour
{
    private TextMeshProUGUI _nameText;
    private TextMeshProUGUI _chatText;
    private GameObject _chatBalloon;
    private Vector3 _originalScale;

    private void Awake()
    {
        _nameText = transform.GetChild(0)?.GetChild(1)?.GetComponent<TextMeshProUGUI>();

        if( _nameText == null )
        {
            Debug.Log($"UserNamePanel[{transform.GetSiblingIndex()}]::Awake : nameText null ref.");
        }

        _chatBalloon = transform.GetChild(0)?.GetChild(2).gameObject;

        if( _chatBalloon == null )
        {
            Debug.Log($"UserNamePanel[{transform.GetSiblingIndex()}]::Awake : chatBalloon null ref");
        }
        else
        {
            _originalScale = _chatBalloon.transform.localScale;
            _chatBalloon.transform.localScale = Vector3.zero;

            _chatText = _chatBalloon.transform.GetChild(0)?.GetComponent<TextMeshProUGUI>();

            if(_chatText == null )
            {
                Debug.Log($"UserNamePanel[{transform.GetSiblingIndex()}]::Awake : chatText null ref");
            }
        }

    }

    public bool IsActive()
    {
        if(transform.childCount == 0)
        {
            return false;
        }

        return transform.GetChild(0).gameObject.activeSelf;
    }
    public void SetName(string name)
    {
        if(_nameText == null)
        {
            return;
        }    
        _nameText.text = name;
    }

    public void SetActive(bool isActive_)
    {
        transform.GetChild(0)?.gameObject.SetActive(isActive_);
    }

    public void Chat(string text)
    {
        if( _chatBalloon == null )
        {
            return;
        }

        _chatText.text = text;
        StartCoroutine(ChatBoxFadeCoroutine());
    }

    IEnumerator ChatBoxFadeCoroutine()
    {
        // 1. 팽창 애니메이션 (0.3초)
        float growTime = 0.3f;
        float timer = 0f;
        while (timer < growTime)
        {
            float t = timer / growTime;
            _chatBalloon.transform.localScale = Vector3.Lerp(Vector3.zero, _originalScale, t);
            timer += Time.deltaTime;
            yield return null;
        }
        _chatBalloon.transform.localScale = _originalScale;

        // 2. 3초 유지
        yield return new WaitForSeconds(3f);

        // 3. 축소 애니메이션 (0.3초)
        float shrinkTime = 0.3f;
        timer = 0f;
        while (timer < shrinkTime)
        {
            float t = timer / shrinkTime;
            _chatBalloon.transform.localScale = Vector3.Lerp(_originalScale, Vector3.zero, t);
            timer += Time.deltaTime;
            yield return null;
        }
        _chatBalloon.transform.localScale = Vector3.zero;
    }
}
