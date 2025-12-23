#version 460
#pragma shader_stage(vertex)

layout(set = 0,binding = 0) uniform trianglePosition{
    vec4 u_Positions[5];
};

layout(location = 0) in vec2 i_Position;
layout(location = 1) in vec4 i_Color;
layout(location = 0) out vec4 o_Color;

void main(){
    gl_Position = vec4(i_Position + u_Positions[gl_InstanceIndex].xy,0,1);
    o_Color = i_Color;
}

