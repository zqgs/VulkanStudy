#version 460
#pragma shader_stage(vertex)

layout(set = 0,binding = 0) uniform UBO_Position{
    vec4 u_Positions[3];
};
layout(std430,set = 0,binding = 1) buffer SSBO_Position{
    vec4 s_Positions[2];
};

layout(location = 0) in vec2 i_Position;
layout(location = 1) in vec4 i_Color;
layout(location = 0) out vec4 o_Color;

void main(){
    uint idx = gl_InstanceIndex;
    vec4 pos;
    if (idx < 3){
        pos = u_Positions[idx];
    }
    else{
        pos = s_Positions[idx - 3];
    }
    gl_Position = vec4(i_Position + pos.xy,0,1);
    o_Color = i_Color;
}

