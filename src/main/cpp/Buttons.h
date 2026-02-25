//
// Created by samma on 2025-12-03.
//
#pragma once
#ifndef DRUMCOMP_BUTTONS_H
#define DRUMCOMP_BUTTONS_H

#endif //DRUMCOMP_BUTTONS_H

#include "raymob.h"

class appButton{
public:
    typedef void(*eventFunction)(int, AudioStream&);


    // members:

    int positionx;
    int positiony;
    float Size;
    Color normColor;
    Color selectColor;
    Color playColor;
    eventFunction onClick;
    int sampleIndex;
    AudioStream& Stream;
    bool clicked = false;

    // constructor:

    appButton(int posx, int posy, int size, Color normcolor, Color selectcolor,
              Color playcolor, eventFunction funcPointer, int sampleindex, AudioStream& stream) :
            positionx(posx),
            positiony(posy),
            Size(size),
            normColor(normcolor),
            selectColor(selectcolor),
            playColor(playcolor),
            onClick(funcPointer),
            sampleIndex(sampleindex),
            Stream(stream)
    {}

    // methods:

    void OnClick(){
        onClick(sampleIndex, Stream);
    }

    bool Touched(Vector2 fingerPos) {
        if (fingerPos.x > positionx  && fingerPos.x < positionx + Size &&
            fingerPos.y > positiony  && fingerPos.y < positiony + Size && !clicked){
            clicked = true; // dit is om te voorkomen dat de button vaker word geklikt in 1 aanraak event
            return true;
        }else if ((fingerPos.x > positionx  && fingerPos.x < positionx + Size &&
                   fingerPos.y > positiony  && fingerPos.y < positiony + Size) && clicked){

            return false;
        }else if (!(fingerPos.x > positionx  && fingerPos.x < positionx + Size &&
                  fingerPos.y > positiony  && fingerPos.y < positiony + Size) && clicked){
            clicked = false; // reset button clicked state to false if finger is not touching
            return false;
        }else{
            return false;
        }
    }
};



///// GRAPH

class appGraph{
public:
    // members:

    int positionx;
    int positiony;
    int Size_x;
    int Size_y;
    float * Data;
    int Datasize;

    // constructor:

    appGraph(int posx, int posy, int sizex, int sizey, float * data, int datasize) :
            positionx(posx),
            positiony(posy),
            Size_x(sizex),
            Size_y(sizey),
            Data(data),
            Datasize(datasize)
    {}

    // methods:

    void onShow(){
        DrawRectangle(positionx, positiony, Size_x, Size_y, WHITE);
        DrawRectangleLines(positionx, positiony, Size_x, Size_y, BLACK);
        DrawPlot();
    }

private:
    int step;
    int invSharpness = 2; // how many pixels to skip when drawing a line

    int Cap(float input){
        if (input > Size_y/2) return Size_y/2;
        else if (input < -Size_y/2) return -Size_y/2;
        else return input;
    }

    void DrawPlot(){
        step = invSharpness*(Datasize/Size_x);
        for (int i = 0; i < Size_x/invSharpness; ++i) {
            DrawLine(positionx + i*invSharpness, Cap(Data[i*step]*(Size_y/2)) + positiony + Size_y/2,
                     positionx + (i+1)*invSharpness, Cap(Data[(i+1)*step]*(Size_y/2)) + positiony + Size_y/2, BLACK);
        }
    }
};

Vector2 GetTouchPosGood(int index){
    if (GetTouchPointCount() > 0) {
        return GetTouchPosition(index);
    }
    else return {0,0};
}



