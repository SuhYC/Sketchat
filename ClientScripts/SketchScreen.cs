using System.Collections;
using System.Collections.Generic;
using System.Threading.Tasks;
using UnityEngine;
using UnityEngine.UI;

public class SketchScreen : MonoBehaviour
{

    private static SketchScreen _instance;
    public static SketchScreen Instance
    {
        get { return _instance; }
    }


    private RawImage canvasImage;
    public const int canvasWidth = 512, canvasHeight = 512;

    public Color drawColor = Color.black;
    public float brushSize = 1.0f;

    private Texture2D texture;

    private Vector2Int? lastDrawPosition = null;
    private ushort drawnum = 0;

    private void Awake()
    {
        _instance = this;
    }

    void Start()
    {
        canvasImage = GetComponent<RawImage>();

        if(canvasImage == null)
        {
            Debug.Log($"SketchScreen::Start : canvas image null ref.");
        }

        texture = new Texture2D(canvasWidth, canvasHeight);
        texture.filterMode = FilterMode.Point; // 뭉개지지 않게

        ClearCanvas(Color.white);
        canvasImage.texture = texture;
    }


    void Update()
    {
        if (Input.GetMouseButton(0))
        {
            Vector2 localPoint;
            if (RectTransformUtility.ScreenPointToLocalPointInRectangle(
                canvasImage.rectTransform, Input.mousePosition, null, out localPoint))
            {
                Rect rect = canvasImage.rectTransform.rect;

                if (!rect.Contains(localPoint) && lastDrawPosition == null)
                {
                    return;
                }

                float px = (localPoint.x - rect.x) / rect.width;
                float py = (localPoint.y - rect.y) / rect.height;

                int x = Mathf.Clamp((int)(px * texture.width), 0, texture.width - 1);
                int y = Mathf.Clamp((int)(py * texture.height), 0, texture.height - 1);
                Vector2Int currentPos = new Vector2Int(x, y);

                if (lastDrawPosition == null ||
                    currentPos.x != lastDrawPosition?.x || currentPos.y != lastDrawPosition?.y)
                {
                    if(lastDrawPosition == null)
                    {
                        Task<bool> startTask = PacketMaker.Instance.ReqDrawStart();
                        startTask.Wait();
                    }

                    Task<bool> task = PacketMaker.Instance.ReqDraw(drawnum, currentPos, brushSize, drawColor);
                    task.Wait();
                }

                lastDrawPosition = currentPos;
            }
        }
        else if (lastDrawPosition != null)
        {
            Task<bool> task = PacketMaker.Instance.ReqDrawEnd(drawnum);
            task.Wait();

            drawnum++;
            lastDrawPosition = null;
        }
    }

    public void DrawAt(int x, int y, float brushSize_, Color color_)
    {
        for (int i = -(int)brushSize_; i <= brushSize_; i++)
        {
            for (int j = -(int)brushSize_; j <= brushSize_; j++)
            {
                if (i * i + j * j <= brushSize_ * brushSize_) // 원 안에 있는 픽셀만
                {
                    int dx = x + i;
                    int dy = y + j;

                    if (dx >= 0 && dx < texture.width && dy >= 0 && dy < texture.height)
                    {
                        texture.SetPixel(dx, dy, color_);
                    }
                }
            }
        }

        texture.Apply();
    }

    public void ClearCanvas(Color bgColor)
    {
        Color[] fillColor = new Color[texture.width * texture.height];
        for (int i = 0; i < fillColor.Length; i++)
            fillColor[i] = bgColor;

        RenewCanvas(fillColor);
    }

    public void RenewCanvas(Color[] colors)
    {
        texture.SetPixels(colors);
        texture.Apply();

        return;
    }

    public void DrawLine(Vector2Int from, Vector2Int to, float brushSize_, Color color_)
    {
        int dx = Mathf.Abs(to.x - from.x);
        int dy = Mathf.Abs(to.y - from.y);
        int steps = Mathf.Max(dx, dy);

        for (int i = 0; i <= steps; i++)
        {
            float t = steps == 0 ? 0 : (float)i / steps;
            int x = Mathf.RoundToInt(Mathf.Lerp(from.x, to.x, t));
            int y = Mathf.RoundToInt(Mathf.Lerp(from.y, to.y, t));
            DrawAt(x, y, brushSize_, color_);
        }
    }
}
