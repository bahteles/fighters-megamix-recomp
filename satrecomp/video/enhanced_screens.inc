// SPDX-License-Identifier: GPL-3.0-only
// Stream full-screen art independently from the bounded fighter texture atlas.
namespace {
bool ReadScreenMember(const std::string &archive,const std::string &wanted,std::string &output){
    std::ifstream file(std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(archive.c_str()))),std::ios::binary);
    while(file){
        uint8_t header[30];if(!file.read(reinterpret_cast<char*>(header),30)||memcmp(header,"PK\x03\x04",4))return false;
        auto word=[&](int i){return uint32_t(header[i])|uint32_t(header[i+1])<<8;};
        auto number=[&](int i){return word(i)|word(i+2)<<16;};
        uint32_t packed=number(18),bytes=number(22),method=word(8),crc=number(14);
        if(word(6)&~0x800u||!word(26)||word(26)>256)return false;
        std::string name(word(26),'\0');file.read(name.data(),name.size());file.seekg(word(28),std::ios::cur);
        if(name!=wanted){file.seekg(packed,std::ios::cur);continue;}
        if(!bytes||bytes>64*1024*1024||packed>64*1024*1024)return false;
        output.resize(bytes);
        if(method==0&&packed==bytes)file.read(output.data(),bytes);
        else if(method==8){
            std::string input(packed,'\0');if(!file.read(input.data(),packed))return false;
            z_stream stream{};stream.next_in=reinterpret_cast<Bytef*>(input.data());stream.avail_in=packed;
            stream.next_out=reinterpret_cast<Bytef*>(output.data());stream.avail_out=bytes;
            if(inflateInit2(&stream,-MAX_WBITS)!=Z_OK)return false;
            int result=inflate(&stream,Z_FINISH);inflateEnd(&stream);
            if(result!=Z_STREAM_END||stream.total_out!=bytes||stream.total_in!=packed)return false;
        }else return false;
        return bool(file)&&crc32(0,reinterpret_cast<const Bytef*>(output.data()),bytes)==crc;
    }
    return false;
}
}

bool Gpu::UploadScreenPixels(const void *pixels,uint32_t width,uint32_t height){
    SDL_GPUTexture *texture=Texture(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,width,height,false);
    if(!texture)return false;
    SDL_GPUTransferBufferCreateInfo info{};info.usage=SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;info.size=width*height*4;
    SDL_GPUTransferBuffer *transfer=SDL_CreateGPUTransferBuffer(m_device,&info);bool uploaded=false;
    if(transfer)if(void *mapped=SDL_MapGPUTransferBuffer(m_device,transfer,false)){
        memcpy(mapped,pixels,info.size);SDL_UnmapGPUTransferBuffer(m_device,transfer);
        if(auto *cmd=SDL_AcquireGPUCommandBuffer(m_device)){
            SDL_GPUTextureTransferInfo source{transfer,0,width,height};SDL_GPUTextureRegion region{};
            region.texture=texture;region.w=width;region.h=height;region.d=1;
            auto *pass=SDL_BeginGPUCopyPass(cmd);SDL_UploadToGPUTexture(pass,&source,&region,false);SDL_EndGPUCopyPass(pass);
            uploaded=SDL_SubmitGPUCommandBuffer(cmd);
        }
    }
    if(transfer)SDL_ReleaseGPUTransferBuffer(m_device,transfer);
    if(!uploaded){Release(texture);return false;}
    Release(m_screenTexture);m_screenTexture=texture;return true;
}

void Gpu::UpdateRecordArt(const Frame &frame,int32_t (&rect)[4],int32_t (&meta)[4]){
    if(frame.height!=224||frame.width<704)return;
    const uint32_t green=frame.layers[3][size_t(80)*frame.width+frame.width/2];
    if(!(green>>24)||(green&255)>32||((green>>8)&255)<60||((green>>16)&255)<40||((green>>8)&255)<=((green>>16)&255))return;
    if(!m_recordsAttempted){
        m_recordsAttempted=true;std::string data,header;
        if(ReadScreenMember(m_hdPackPath,"screens/record-fonts.bin",data)&&data.size()>=12&&data.compare(0,8,"FMMRF001")==0){
            uint32_t count;memcpy(&count,data.data()+8,4);size_t at=12;
            for(uint32_t i=0;i<std::min(count,512u)&&at+8<=data.size();i++){
                RecordGlyph glyph{};memcpy(&glyph.width,data.data()+at,4);memcpy(&glyph.crc,data.data()+at+4,4);at+=8;
                size_t bytes=size_t(glyph.width)*16*16*4;
                if(!glyph.width||glyph.width>200||at+bytes>data.size())break;
                glyph.pixels.assign(data.begin()+at,data.begin()+at+bytes);at+=bytes;m_recordGlyphs.push_back(std::move(glyph));
            }
        }
        if(ReadScreenMember(m_hdPackPath,"screens/records-header.rgba",header)&&header.size()==2816*96*4)m_recordsHeader.assign(header.begin(),header.end());
    }
    if(m_recordGlyphs.empty()||m_recordsHeader.empty())return;
    const uint32_t left=(frame.width-704)/2;
    std::vector<uint32_t> original(704*224);
    for(uint32_t y=0;y<224;y++)std::copy_n(frame.layers[4]+size_t(y)*frame.width+left,704,original.data()+y*704);
    uint32_t signature=crc32(0,reinterpret_cast<const Bytef*>(original.data()),original.size()*4);
    if(m_screenUploaded!=-2||m_recordsCrc!=signature){
        std::vector<uint8_t> pixels(2816*896*4);
        for(uint32_t y=0;y<896;y++)for(uint32_t x=0;x<2816;x++){
            uint32_t p=original[(y/4)*704+x/4];size_t at=(size_t(y)*2816+x)*4;
            pixels[at]=p;pixels[at+1]=p>>8;pixels[at+2]=p>>16;pixels[at+3]=p>>24?255:0;
        }
        std::copy(m_recordsHeader.begin(),m_recordsHeader.end(),pixels.begin());
        unsigned replaced=0;
        auto replace=[&](int x,int y,uint32_t width){
            if(x<0||x+int(width)>704)return;
            std::vector<uint8_t> mask(width*16);
            for(int yy=0;yy<16;yy++)for(uint32_t xx=0;xx<width;xx++)mask[yy*width+xx]=original[(y+yy)*704+x+xx]>>24?1:0;
            uint32_t crc=crc32(0,mask.data(),mask.size());
            for(const auto &glyph:m_recordGlyphs)if(glyph.width==width&&glyph.crc==crc){
                for(int yy=0;yy<64;yy++)memcpy(pixels.data()+(size_t(y*4+yy)*2816+x*4)*4,glyph.pixels.data()+yy*width*16,width*16);
                uint32_t peak=0;
                for(int yy=0;yy<16;yy++)for(uint32_t xx=0;xx<width;xx++){
                    uint32_t p=original[(y+yy)*704+x+xx];if(p>>24)peak=std::max({peak,p&255u,(p>>8)&255u,(p>>16)&255u});
                }
                if(peak<248)for(int yy=0;yy<64;yy++)for(uint32_t xx=0;xx<width*4;xx++){
                    size_t at=(size_t(y*4+yy)*2816+x*4+xx)*4;
                    for(int channel=0;channel<3;channel++)pixels[at+channel]=uint8_t(uint32_t(pixels[at+channel])*peak/248);
                }
                replaced++;return;
            }
        };
        for(int row=0;row<4;row++){
            int y=72+row*32;replace(48,y,24);replace(80,y,84);
            for(int i=0;i<3;i++)replace(184+24*i,y,24);
            for(int i=0;i<8;i++)replace(280+24*i,y,24);
            replace(486,y,36);replace(526,y,176);
        }
        if(replaced<16||!UploadScreenPixels(pixels.data(),2816,896))return;
        m_screenUploaded=-2;m_recordsCrc=signature;
        fprintf(stderr,"Enhanced records: %u fields\n",replaced);
    }
    rect[0]=int32_t(left);rect[1]=0;rect[2]=704;rect[3]=224;
    meta[0]=4;meta[1]=3;meta[2]=7;meta[3]=0;
}

void Gpu::UpdateScreenArt(const Frame &frame,int32_t (&rect)[4],int32_t (&meta)[4]){
    if(!m_hdEnabled)return;
    if(!m_screenAttempted){
        m_screenAttempted=true;std::string bytes;
        if(ReadScreenMember(m_hdPackPath,"screens/catalog.bin",bytes)&&bytes.size()>=12&&bytes.compare(0,8,"FMMUI003")==0){
            size_t at=8;
            auto number=[&](){uint32_t value=0;if(at+4<=bytes.size())memcpy(&value,bytes.data()+at,4);at+=4;return value;};
            uint32_t count=number();
            for(uint32_t i=0;i<std::min(count,256u)&&at+48<=bytes.size();i++){
                ScreenArt art{};
                art.layer=number();art.sourceWidth=number();art.sourceHeight=number();art.kind=number();art.crc=number();
                art.left=int32_t(number());art.top=int32_t(number());art.width=int32_t(number());art.height=int32_t(number());
                art.columns=number();art.lines=number();uint32_t length=number();
                if(art.layer>3||!art.sourceWidth||art.sourceWidth>1024||!art.sourceHeight||art.sourceHeight>512||art.kind<1||art.kind>2||art.width<1||art.height<1||!art.columns||art.columns>4096||!art.lines||art.lines>4096||length>200||at+length>bytes.size())break;
                art.member=bytes.substr(at,length);at+=length;m_screenArt.push_back(std::move(art));
            }
        }
    }
    uint32_t hashes[2]{};bool have[2]{};
    for(size_t index=0;index<m_screenArt.size();index++){
        const auto &art=m_screenArt[index];
        if(frame.height!=art.sourceHeight||frame.width<art.sourceWidth||!frame.layers[art.layer+1])continue;
        uint32_t key=art.kind-1;
        if(!have[key]){
            std::vector<uint8_t> signature;signature.reserve(size_t(art.sourceWidth)*art.sourceHeight*(art.kind==1?1:3));
            uint32_t left=(frame.width-art.sourceWidth)/2;
            for(uint32_t y=0;y<art.sourceHeight;y++)for(uint32_t x=0;x<art.sourceWidth;x++){
                uint32_t p=frame.layers[art.layer+1][size_t(y)*frame.width+left+x];
                uint8_t r=p,g=p>>8,b=p>>16,a=p>>24;
                if(art.kind==1)signature.push_back(a&&std::max({r,g,b})>64?(r>int(b)+30?2:1):0);
                else{signature.push_back(r);signature.push_back(g);signature.push_back(b);}
            }
            hashes[key]=crc32(0,signature.data(),signature.size());have[key]=true;
        }
        if(hashes[key]!=art.crc)continue;
        if(m_screenUploaded!=int(index)){
            std::string pixels;
            if(!ReadScreenMember(m_hdPackPath,art.member,pixels)||pixels.size()!=size_t(art.columns)*art.lines*4)continue;
            SDL_GPUTexture *texture=Texture(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,art.columns,art.lines,false);
            if(!texture)continue;
            SDL_GPUTransferBufferCreateInfo info{};info.usage=SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;info.size=uint32_t(pixels.size());
            SDL_GPUTransferBuffer *transfer=SDL_CreateGPUTransferBuffer(m_device,&info);
            bool uploaded=false;
            if(transfer)if(void *mapped=SDL_MapGPUTransferBuffer(m_device,transfer,false)){
                memcpy(mapped,pixels.data(),pixels.size());SDL_UnmapGPUTransferBuffer(m_device,transfer);
                if(auto *cmd=SDL_AcquireGPUCommandBuffer(m_device)){
                    SDL_GPUTextureTransferInfo source{transfer,0,art.columns,art.lines};SDL_GPUTextureRegion region{};
                    region.texture=texture;region.w=art.columns;region.h=art.lines;region.d=1;
                    auto *pass=SDL_BeginGPUCopyPass(cmd);SDL_UploadToGPUTexture(pass,&source,&region,false);SDL_EndGPUCopyPass(pass);
                    uploaded=SDL_SubmitGPUCommandBuffer(cmd);
                }
            }
            if(transfer)SDL_ReleaseGPUTransferBuffer(m_device,transfer);
            if(!uploaded){Release(texture);continue;}
            Release(m_screenTexture);m_screenTexture=texture;m_screenUploaded=int(index);
            fprintf(stderr,"Enhanced screen: %s\n",art.member.c_str());
        }
        rect[0]=int32_t(frame.width)/2+art.left;rect[1]=art.top;rect[2]=art.width;rect[3]=art.height;
        meta[0]=int32_t(art.layer)+1;meta[1]=art.kind;meta[2]=7;meta[3]=0;
        // Static credit cards have a navy back screen. Replay footage keeps its own background.
        uint32_t back=frame.back[frame.height/2];
        if(art.kind==1){
            if((back&0xffff)==0&&(back&0xff0000)&&((back>>16)&255)<100)meta[3]=int32_t(back&0xffffff);
            for(int layer=0;layer<5;layer++)if(layer!=int(art.layer+1)){
                uint32_t p=frame.layers[layer][size_t(frame.height/2)*frame.width+frame.width/2];
                if((p>>24)&&(p&0xffff)==0&&((p>>16)&255)>0&&((p>>16)&255)<100)meta[3]=int32_t(p&0xffffff);
            }
        }
        return;
    }
    UpdateRecordArt(frame,rect,meta);
}
