using System.Collections;
using System.Collections.Generic;
using Unity.VisualScripting;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;

public class ColorPalette : MonoBehaviour, IPointerClickHandler
{
    private RawImage m_paletteImage;
    private Image m_ColorDisplay;
    private Texture2D m_texture;

    private Color[] colors; // 색상 배열

    private int textureSize = 128;  // 텍스처 크기

    void Awake()
    {
        m_paletteImage = GetComponent<RawImage>();
        m_ColorDisplay = transform.GetChild(0)?.GetComponent<Image>();

        if (m_paletteImage == null)
        {
            Debug.Log($"Colorpalette::Awake : palette null ref.");
        }

        if(m_ColorDisplay == null)
        {
            Debug.Log($"Colorpalette::Awake : display null ref.");
        }

        colors = new Color[]
        {
            Color.red, Color.green, Color.blue, Color.yellow,
            Color.cyan, Color.magenta, Color.white, Color.black,
            new Color(0.5f, 0.5f, 0f), new Color(0.5f, 0f, 0.5f), new Color(0f, 0.5f, 0.5f),
            new Color(0.7f, 0.7f, 0f), new Color(0.7f, 0f, 0.7f), new Color(0f, 0.7f, 0.7f),
            new Color(0.5f, 0.25f, 0f), new Color(0.25f, 0.5f, 0f)
        };

        m_texture = new Texture2D(textureSize, textureSize);
        m_paletteImage.texture = m_texture;
    }

    void Start()
    {
        CreatePalette();
    }

    
    void CreatePalette()
    {
        for (int y = 0; y < 4; y++)  // 4행
        {
            for (int x = 0; x < 4; x++)  // 4열
            {
                int colorIndex = y * 4 + x;  // 색상 인덱스 계산
                Color color = colors[colorIndex];

                // 각 64x64 구역에 색상을 채운다
                for (int j = y * textureSize / 4; j < (y + 1) * textureSize / 4; j++)
                {
                    for (int i = x * textureSize / 4; i < (x + 1) * textureSize / 4; i++)
                    {
                        m_texture.SetPixel(i, j, color);
                    }
                }
            }
        }

        m_texture.Apply();  // 변경사항 적용
    }

    public void OnPointerClick(PointerEventData eventData)
    {
        // 클릭된 위치 (RawImage 내에서의 좌표)
        Vector2 localPos;
        RectTransform rect = transform.GetComponent<RectTransform>();

        RectTransformUtility.ScreenPointToLocalPointInRectangle(rect, Input.mousePosition, null, out localPos);

        // 클릭된 위치를 텍스처 좌표로 변환
        int x = Mathf.FloorToInt((localPos.x + rect.rect.width / 2) / (textureSize / 4));
        int y = Mathf.FloorToInt((localPos.y + rect.rect.height / 2) / (textureSize / 4));

        // 범위 체크
        if (x >= 0 && x < 4 && y >= 0 && y < 4)
        {
            int colorIndex = y * 4 + x;  // 선택된 색상 인덱스
            Color selectedColor = colors[colorIndex];  // 색상 가져오기

            // 선택된 색상 표시
            m_ColorDisplay.color = selectedColor;
            SketchScreen.Instance.drawColor = selectedColor;
        }
    }
}
