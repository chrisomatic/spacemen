#version 400 core

in vec2 tex_coord0;
in vec3 color0;
in float opacity0;
flat in uint image_index0;
flat in uint ignore_light0;
flat in uint mask_color0;
flat in uint blending_mode0;
in vec2 to_light_vector[16];

out vec4 outColor;

uniform sampler2D images[16];
uniform vec3 ambient_color;
uniform vec3 light_color[16];
uniform vec3 light_atten[16];

void main() {

    // texture color
    vec4 tex_color = texture(images[image_index0],tex_coord0.xy);
    if(tex_color == vec4(1.0,0.0,1.0,1.0))
        discard;

    if(mask_color0 == uint(1))
    {
        // replace red pixels with color0
        if(tex_color.r > 0.0 && tex_color.g == 0.0 && tex_color.b == 0.00)
        {
            float amount = tex_color.r / 1.0;
            tex_color = vec4(color0.r * amount, color0.g * amount, color0.b * amount, tex_color.a);
        }
    }

    vec3 total_diffuse = vec3(0.0);

    // point lights
    for(int i = 0; i < 16; ++i)
    {
        float dist_to_light = length(to_light_vector[i]);
        float atten_factor = light_atten[i].x + light_atten[i].y*dist_to_light + light_atten[i].z*dist_to_light*dist_to_light;

        total_diffuse = total_diffuse + (light_color[i] / atten_factor);
    }

    total_diffuse = min(total_diffuse,vec3(1.0,1.0,1.0)); // cap the total diffuse
    total_diffuse = max(total_diffuse, ambient_color);

    vec4 my_color;


    if(ignore_light0 == uint(1))
    {
        my_color = vec4(tex_color.rgb,tex_color.a*opacity0);
    }
    else
    {
        my_color = vec4(total_diffuse, opacity0)*tex_color;
    }

    if(mask_color0 == uint(0))
        my_color.rgb *= color0.rgb;

    my_color.rgb *= my_color.a;

    if(blending_mode0 == uint(1))
    {
        my_color.a = 0.0; // additive
    }

    outColor = my_color;
}
