using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

/// <summary>
/// 한번의 그림동작을 표현하는 클래스.
/// Undo로 한번에 취소되는 단위로,
/// 한가지의 색과, 한가지의 크기, 그리고 여러개의 좌표로 이루어져있다.
/// 지정한 색과 크기로 직선을 구성하여 연속된 두 좌표를 직선으로 표현하면 된다.
/// </summary>
public class DrawCommand
{
    public Color DrawColor;
    public float DrawWidth;
    public Vector2Int[] vertices;
    public int size;

    public DrawCommand(Color col, float width)
    {
        DrawColor = col;
        DrawWidth = width;
        vertices = new Vector2Int[2];
        size = 0;
    }

    public void Push(Vector2Int vertex)
    {
        if(size == vertices.Length)
        {
            Vector2Int[] newVector = new Vector2Int[size * 2];
            Array.Copy(vertices, newVector, vertices.Length);
            vertices = newVector;
        }
        
        vertices[size] = vertex;
        size++;

        return;
    }
}
