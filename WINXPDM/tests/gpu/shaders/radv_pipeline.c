/* Compile a vertex/fragment pipeline with RADV's offline winsys.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

static void check(VkResult result,const char *name)
{
    if (result!=VK_SUCCESS) { fprintf(stderr,"%s returned %d\n",name,result); exit(1); }
}

static VkShaderModule load(VkDevice device,const char *path)
{
    FILE *f=fopen(path,"rb");
    VkShaderModule module;
    VkShaderModuleCreateInfo info={.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    if (!f || fseek(f,0,SEEK_END)) exit(1);
    long n=ftell(f);
    if (n<20 || n%4 || n>1024*1024 || fseek(f,0,SEEK_SET)) exit(1);
    uint32_t *words=malloc((size_t)n);
    if (!words || fread(words,1,(size_t)n,f)!=(size_t)n) exit(1);
    fclose(f);
    info.codeSize=(size_t)n; info.pCode=words;
    check(vkCreateShaderModule(device,&info,NULL,&module),path);
    free(words);
    return module;
}

int main(int argc,char **argv)
{
    if (argc!=4) { fprintf(stderr,"Usage: radv_pipeline FAMILY VERT.spv FRAG.spv\n"); return 2; }
    const char *family=argv[1];
    if (strcmp(family,"GFX1200") && strcmp(family,"GFX1201")) return 2;
    setenv("RADV_FORCE_FAMILY",family,1);
    setvbuf(stdout,NULL,_IONBF,0);
    VkInstance inst;
    VkApplicationInfo app={.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO,.pApplicationName="WINXPDM pipeline test",.apiVersion=VK_API_VERSION_1_1};
    VkInstanceCreateInfo in={.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,.pApplicationInfo=&app};
    check(vkCreateInstance(&in,NULL,&inst),"vkCreateInstance");
    uint32_t count=0;
    check(vkEnumeratePhysicalDevices(inst,&count,NULL),"vkEnumeratePhysicalDevices");
    if (count!=1) { fprintf(stderr,"Select the RADV ICD only.\n"); return 1; }
    VkPhysicalDevice physical;
    check(vkEnumeratePhysicalDevices(inst,&count,&physical),"vkEnumeratePhysicalDevices");
    VkPhysicalDeviceProperties prop;
    vkGetPhysicalDeviceProperties(physical,&prop);
    printf("Target=%s Driver=%s Vendor=0x%x\n",family,prop.deviceName,prop.vendorID);
    if (prop.vendorID!=0x1002) return 1;
    const float priority=1;
    VkDeviceQueueCreateInfo q={.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,.queueFamilyIndex=0,.queueCount=1,.pQueuePriorities=&priority};
    const char *extension=VK_KHR_PIPELINE_EXECUTABLE_PROPERTIES_EXTENSION_NAME;
    VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR feature={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_EXECUTABLE_PROPERTIES_FEATURES_KHR,.pipelineExecutableInfo=VK_TRUE};
    VkDeviceCreateInfo dc={.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,.pNext=&feature,.queueCreateInfoCount=1,.pQueueCreateInfos=&q,
                           .enabledExtensionCount=1,.ppEnabledExtensionNames=&extension};
    VkDevice device;
    check(vkCreateDevice(physical,&dc,NULL,&device),"vkCreateDevice");
    VkShaderModule vs=load(device,argv[2]),fs=load(device,argv[3]);
    VkPipelineShaderStageCreateInfo stages[2]={
        {.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_VERTEX_BIT,.module=vs,.pName="main"},
        {.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_FRAGMENT_BIT,.module=fs,.pName="main"}
    };
    VkDescriptorSetLayoutBinding image={.binding=0,.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,.descriptorCount=1,.stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT};
    VkDescriptorSetLayoutCreateInfo desc={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,.bindingCount=1,.pBindings=&image};
    VkDescriptorSetLayout ds;
    check(vkCreateDescriptorSetLayout(device,&desc,NULL,&ds),"vkCreateDescriptorSetLayout");
    VkPipelineLayoutCreateInfo plc={.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,.setLayoutCount=1,.pSetLayouts=&ds};
    VkPipelineLayout layout;
    check(vkCreatePipelineLayout(device,&plc,NULL,&layout),"vkCreatePipelineLayout");
    VkAttachmentDescription color={.format=VK_FORMAT_R8G8B8A8_UNORM,.samples=VK_SAMPLE_COUNT_1_BIT,.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp=VK_ATTACHMENT_STORE_OP_STORE,.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference ref={0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub={.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS,.colorAttachmentCount=1,.pColorAttachments=&ref};
    VkRenderPassCreateInfo rp={.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,.attachmentCount=1,.pAttachments=&color,.subpassCount=1,.pSubpasses=&sub};
    VkRenderPass pass;
    check(vkCreateRenderPass(device,&rp,NULL,&pass),"vkCreateRenderPass");
    VkVertexInputBindingDescription binding={0,36,VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attrs[]={{0,0,VK_FORMAT_R32G32B32_SFLOAT,0},{1,0,VK_FORMAT_R32G32B32A32_SFLOAT,12},{2,0,VK_FORMAT_R32G32_SFLOAT,28}};
    VkPipelineVertexInputStateCreateInfo vin={.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,.vertexBindingDescriptionCount=1,.pVertexBindingDescriptions=&binding,
        .vertexAttributeDescriptionCount=3,.pVertexAttributeDescriptions=attrs};
    VkPipelineInputAssemblyStateCreateInfo ia={.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    VkViewport viewport={0,0,64,64,0,1};
    VkRect2D rect={{0,0},{64,64}};
    VkPipelineViewportStateCreateInfo vp={.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,.viewportCount=1,.pViewports=&viewport,.scissorCount=1,.pScissors=&rect};
    VkPipelineRasterizationStateCreateInfo raster={.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,.polygonMode=VK_POLYGON_MODE_FILL,.lineWidth=1};
    VkPipelineMultisampleStateCreateInfo ms={.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT};
    VkPipelineColorBlendAttachmentState blend={.blendEnable=VK_TRUE,.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA,.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE,.dstAlphaBlendFactor=VK_BLEND_FACTOR_ZERO,.colorWriteMask=15};
    VkPipelineColorBlendStateCreateInfo color_state={.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,.attachmentCount=1,.pAttachments=&blend};
    VkGraphicsPipelineCreateInfo state={.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,.flags=VK_PIPELINE_CREATE_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR,
        .stageCount=2,.pStages=stages,.pVertexInputState=&vin,.pInputAssemblyState=&ia,.pViewportState=&vp,.pRasterizationState=&raster,.pMultisampleState=&ms,
        .pColorBlendState=&color_state,.layout=layout,.renderPass=pass};
    VkPipeline pipeline;
    check(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&state,NULL,&pipeline),"vkCreateGraphicsPipelines");
    PFN_vkGetPipelineExecutablePropertiesKHR properties=(PFN_vkGetPipelineExecutablePropertiesKHR)vkGetDeviceProcAddr(device,"vkGetPipelineExecutablePropertiesKHR");
    PFN_vkGetPipelineExecutableInternalRepresentationsKHR internals=(PFN_vkGetPipelineExecutableInternalRepresentationsKHR)vkGetDeviceProcAddr(device,"vkGetPipelineExecutableInternalRepresentationsKHR");
    if (!properties || !internals) return 1;
    VkPipelineInfoKHR pipe_info={.sType=VK_STRUCTURE_TYPE_PIPELINE_INFO_KHR,.pipeline=pipeline};
    uint32_t n=0;
    check(properties(device,&pipe_info,&n,NULL),"executable count");
    VkPipelineExecutablePropertiesKHR *list=calloc(n,sizeof(*list));
    if (!list) return 1;
    for (uint32_t i=0;i<n;++i) list[i].sType=VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_PROPERTIES_KHR;
    check(properties(device,&pipe_info,&n,list),"executables");
    VkShaderStageFlags verified=0;
    for (uint32_t i=0;i<n;i++) {
        VkPipelineExecutableInfoKHR bin={.sType=VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_INFO_KHR,.pipeline=pipeline,.executableIndex=i};
        uint32_t nr=0;
        check(internals(device,&bin,&nr,NULL),"representation count");
        VkPipelineExecutableInternalRepresentationKHR *reps=calloc(nr,sizeof(*reps));
        if (!reps) return 1;
        for (uint32_t j=0;j<nr;j++) reps[j].sType=VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_INTERNAL_REPRESENTATION_KHR;
        check(internals(device,&bin,&nr,reps),"representations");
        for (uint32_t j=0;j<nr;j++) {
            if (!reps[j].isText || !strstr(reps[j].name,"Assembly")) continue;
            reps[j].pData=calloc(reps[j].dataSize+1,1);
            if (!reps[j].pData) return 1;
            check(internals(device,&bin,&nr,reps),"assembly");
            const char *assembly=reps[j].pData;
            printf("Executable=%s Stages=0x%x Representation=%s\n%s\n",list[i].name,list[i].stages,reps[j].name,assembly);
            if (strstr(assembly,"s_endpgm") && (strstr(assembly,"image_sample") || !(list[i].stages&VK_SHADER_STAGE_FRAGMENT_BIT)))
                verified|=list[i].stages;
            free(reps[j].pData); reps[j].pData=NULL;
        }
        free(reps);
    }
    free(list);
    vkDestroyPipeline(device,pipeline,NULL);vkDestroyRenderPass(device,pass,NULL);
    vkDestroyPipelineLayout(device,layout,NULL);vkDestroyDescriptorSetLayout(device,ds,NULL);
    vkDestroyShaderModule(device,vs,NULL);vkDestroyShaderModule(device,fs,NULL);
    vkDestroyDevice(device,NULL);vkDestroyInstance(inst,NULL);
    return (verified&(VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT))==(VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT)?0:1;
}
