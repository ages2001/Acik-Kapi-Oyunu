#version 430
layout(location=0) in vec4 color;
layout(location=1) in vec2 uv;
layout(binding=0) uniform sampler2D imageSampler;
layout(location=0) out vec4 outColor;
void main()
{
    vec4 sampleColor=texture(imageSampler,uv)*color;
    if (sampleColor.a < 0.25) discard;
    outColor=sampleColor;
}
