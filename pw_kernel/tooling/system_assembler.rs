// Copyright 2025 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

use std::collections::{HashMap, HashSet};
use std::fs::{self, File};
use std::io::BufWriter;
use std::path::{Path, PathBuf};
use std::sync::LazyLock;

use anyhow::{anyhow, bail, Context, Result};
use clap::Parser;
use object::build::elf::{
    AttributeTag, AttributesSection, AttributesSubsection, AttributesSubsubsection, Builder,
    Section, SectionData, SectionId,
};
use object::build::{ByteString, Bytes, Id};
use object::{elf, ReadRef};

// Don't copy these sections, as the object writer won't allow multiple
// sections of SYMTAB and STRTAB type.  All other sections should be copied
// even if they're not loaded into memory, as symbols may reference
// these non-alloc sections.
static SKIPPED_APP_SECTIONS: LazyLock<HashSet<&[u8]>> = LazyLock::new(|| {
    [&b".symtab"[..], &b".shstrtab"[..], &b".strtab"[..]]
        .into_iter()
        .collect()
});

#[derive(Debug, Parser)]
struct Args {
    #[arg(long, required(true))]
    kernel: PathBuf,
    #[arg(long("app"))]
    apps: Vec<PathBuf>,
    #[arg(long, required(true))]
    output: PathBuf,
}

struct SystemImage<'data> {
    builder: Builder<'data>,
    tokenized_section: Option<SectionId>,
}

impl<'data> SystemImage<'data> {
    pub fn new<R: ReadRef<'data>>(kernel_bytes: R) -> Result<Self> {
        let builder = Builder::read(kernel_bytes)
            .map_err(|e| anyhow!("Failed to parse kernel image: {e}"))?;

        let mut instance = Self {
            builder,
            tokenized_section: None,
        };

        instance.set_tokenized_section();
        Ok(instance)
    }

    fn write(self, writer: &mut BufWriter<File>) -> Result<()> {
        let mut buffer = object::write::StreamingBuffer::new(writer);

        self.builder
            .write(&mut buffer)
            .map_err(|e| anyhow!("Failed to write system image: {e}"))
    }

    fn add_app_image<'a, R: ReadRef<'a>>(&mut self, app_bytes: R, app_name: &String) -> Result<()> {
        let app_builder =
            Builder::read(app_bytes).map_err(|e| anyhow!("Failed to parse app image: {e}"))?;

        let mut section_map = HashMap::new();
        self.add_app_sections(&app_builder, app_name, &mut section_map)
            .map_err(|e| anyhow!("Failed adding app sections: {e}"))?;
        self.add_app_segments(&app_builder, &section_map)
            .map_err(|e| anyhow!("Failed adding app segments: {e}"))?;
        self.add_app_symbols(&app_builder, app_name, &section_map)
            .map_err(|e| anyhow!("Failed adding app symbols: {e}"))
    }

    fn add_app_sections(
        &mut self,
        app: &Builder,
        app_name: &String,
        section_map: &mut HashMap<usize, SectionId>,
    ) -> Result<()> {
        let mut sections_for_fixup = Vec::new();
        for section in &app.sections {
            let is_tokenizer = Self::is_tokenizer_section(section);
            let mut add_tokenizer_section = false;
            if is_tokenizer {
                add_tokenizer_section = self.update_tokenized_section(section, section_map);
                // Don't add this section, if a tokenized section already exists
                if !add_tokenizer_section {
                    continue;
                }
            }

            // TODO: davidroth - This isn't a problem right now as we don't support
            // debugging of merged self files.  Revisit this once we add debugging
            // support.  Possibly we could move the symbols & strings into merged
            // sections as we do with the tokenized section.
            if SKIPPED_APP_SECTIONS.contains(&section.name.as_slice()) {
                // println!("Skipping section '{}'", section.name);
                continue;
            }

            let new_section = self.builder.sections.add();
            section_map.insert(section.id().index(), new_section.id());

            if add_tokenizer_section {
                self.tokenized_section = Some(new_section.id());
                // Don't rename tokenizer section if we're adding one.
                new_section.name = ByteString::from(section.name.to_vec());
            } else {
                let name = format!("{}.{}", section.name, app_name);
                new_section.name = name.into_bytes().into();
            }

            if section.sh_link_section.is_some() || section.sh_info_section.is_some() {
                sections_for_fixup.push(new_section.id());
            }

            Self::copy_section(section_map, section, new_section)?;

            // println!("Added app section '{:?}'", new_section);
        }

        // Now that all the sections have been added, go back and update any
        // references to SectionIds.
        for section_id in sections_for_fixup {
            let section = self.builder.sections.get_mut(section_id);
            if let Some(id) = section.sh_link_section {
                section.sh_link_section = Self::get_mapped_section_id(section_map, id)?;
            }
            if let Some(id) = section.sh_info_section {
                section.sh_info_section = Self::get_mapped_section_id(section_map, id)?;
            }
        }

        Ok(())
    }

    fn copy_section(
        section_map: &mut HashMap<usize, SectionId>,
        src: &Section,
        dst: &mut Section<'data>,
    ) -> Result<()> {
        dst.sh_type = src.sh_type;
        dst.sh_flags = src.sh_flags;
        // Copy sh_addr and sh_offset.  They will be updated if they're
        // added to a segment.
        dst.sh_addr = src.sh_addr;
        dst.sh_offset = src.sh_offset;
        dst.sh_size = src.sh_size;
        // OK to copy the original SectiondId's. They will be re-mapped
        // after all the sections have been added.
        dst.sh_link_section = src.sh_link_section;
        dst.sh_info_section = src.sh_info_section;
        dst.sh_addralign = src.sh_addralign;
        dst.sh_entsize = src.sh_entsize;
        dst.data = match &src.data {
            SectionData::Data(data) => SectionData::Data(Bytes::from(data.to_vec())),
            SectionData::UninitializedData(data) => SectionData::UninitializedData(*data),
            SectionData::Attributes(data) => {
                Self::copy_section_attributes(section_map, data).unwrap()
            }
            SectionData::SectionString => SectionData::SectionString,
            SectionData::Symbol => SectionData::Symbol,
            SectionData::SymbolSectionIndex => SectionData::SymbolSectionIndex,
            SectionData::String => SectionData::String,
            SectionData::DynamicSymbol => SectionData::DynamicSymbol,
            SectionData::DynamicString => SectionData::DynamicString,
            SectionData::Hash => SectionData::Hash,
            SectionData::GnuHash => SectionData::GnuHash,
            SectionData::GnuVersym => SectionData::GnuVersym,
            SectionData::GnuVerdef => SectionData::GnuVerdef,
            SectionData::GnuVerneed => SectionData::GnuVerneed,
            _ => unreachable!("Unsupported section data type: {:?}", src.data),
        };

        Ok(())
    }

    // Copy the section attributes which may GNU or other vendor-specific attributes. Need to
    // deep copy as the object crate does not provide a Copy and if an attribute tag points
    // to a section id, it need to be remapped to the new section id in the merged elf.
    fn copy_section_attributes(
        section_map: &mut HashMap<usize, SectionId>,
        data: &AttributesSection,
    ) -> Result<SectionData<'data>, ()> {
        let mut attributes_section = AttributesSection::new();
        for subsection in &data.subsections {
            let mut attributes_subsection =
                AttributesSubsection::new(ByteString::from(subsection.vendor.to_vec()));
            for subsubsection in &subsection.subsubsections {
                let tag = match &subsubsection.tag {
                    AttributeTag::File => AttributeTag::File,
                    AttributeTag::Section(section_tag) => {
                        let mut tag_sections = Vec::new();
                        // Remap the section ids to the new ids in the merged elf.
                        for section_id in section_tag {
                            let mapped_id = Self::get_mapped_section_id(section_map, *section_id);
                            tag_sections.push(mapped_id.unwrap().expect("Section attribute copy"));
                        }
                        AttributeTag::Section(tag_sections)
                    }
                    AttributeTag::Symbol(symbol_tag) => AttributeTag::Symbol(symbol_tag.to_vec()),
                };

                let attributes_subsubsection = AttributesSubsubsection {
                    tag,
                    data: Bytes::from(subsubsection.data.to_vec()),
                };
                attributes_subsection
                    .subsubsections
                    .push(attributes_subsubsection);
            }
            attributes_section.subsections.push(attributes_subsection);
        }
        Ok(SectionData::Attributes(attributes_section))
    }

    fn add_app_segments(
        &mut self,
        app: &Builder,
        section_map: &HashMap<usize, SectionId>,
    ) -> Result<()> {
        for segment in &app.segments {
            if !segment.is_load() {
                // println!("Skipping non-load segment {:?}", segment.id());
                continue;
            }
            let new_segment = self
                .builder
                .segments
                .add_load_segment(segment.p_flags, segment.p_align);
            // Make sure to preserve the addresses where the original
            // segment is loaded, as add_load_segment() will change
            // them.
            // TODO: davidroth - investigate submitting an upstream
            // path to make this more robust and not require fixup.
            new_segment.p_paddr = segment.p_paddr;
            new_segment.p_vaddr = segment.p_vaddr;
            for section_id in &segment.sections {
                let mapped_section_id = Self::get_mapped_section_id(section_map, *section_id)?;
                let section = self.builder.sections.get_mut(mapped_section_id.unwrap());
                new_segment.append_section(section);
            }
            // println!("Added segment {:?}", new_segment.id());
        }

        Ok(())
    }

    fn add_app_symbols(
        &mut self,
        app: &Builder,
        app_name: &String,
        section_map: &HashMap<usize, SectionId>,
    ) -> Result<()> {
        for symbol in &app.symbols {
            // println!("Adding app symbol: {:?}", symbol);
            let new_symbol = self.builder.symbols.add();
            if symbol.st_bind() == elf::STB_GLOBAL {
                let new_name = format!("{}_{}", symbol.name, app_name);
                new_symbol.name = new_name.into_bytes().into();
            }
            if symbol.section.is_some() {
                new_symbol.section =
                    Self::get_mapped_section_id(section_map, symbol.section.unwrap())?;
            }
            new_symbol.st_info = symbol.st_info;
            new_symbol.st_other = symbol.st_other;
            new_symbol.st_shndx = symbol.st_shndx;
            new_symbol.st_value = symbol.st_value;
            new_symbol.st_size = symbol.st_size;
            new_symbol.version = symbol.version;
            new_symbol.version_hidden = symbol.version_hidden;
        }
        Ok(())
    }

    fn get_mapped_section_id(
        section_map: &HashMap<usize, SectionId>,
        id: SectionId,
    ) -> Result<Option<SectionId>> {
        Self::get_mapped_section_id_from_index(section_map, id.index())
    }

    fn get_mapped_section_id_from_index(
        section_map: &HashMap<usize, SectionId>,
        id: usize,
    ) -> Result<Option<SectionId>> {
        match section_map.get(&id) {
            Some(mapped_id) => {
                // println!("Mapped SectionId {:?} to {:?}", id, mapped_id);
                Ok(Some(*mapped_id))
            }
            None => bail!("No mapping for {:?}", id),
        }
    }

    // Within the combined elf, there can only be one tokenized section
    // that must be called `.pw_tokenizer.entries` as this is what the
    // de-tokenization tooling expects.
    // The tokenization database is designed to be appended, so for
    // each token section we encounter in an elf, we just append the
    // bytes to the end of the first token section we encounter.
    fn set_tokenized_section(&mut self) {
        for section in &mut self.builder.sections {
            let is_tokenizer = Self::is_tokenizer_section(section);
            if is_tokenizer {
                // println!("Tokenized section: {:?}", section);
                self.tokenized_section = Some(section.id());
                break;
            }
        }
    }

    fn update_tokenized_section(
        &mut self,
        section: &Section,
        section_map: &mut HashMap<usize, SectionId>,
    ) -> bool {
        let mut add_tokenizer_section = false;
        match self.tokenized_section {
            Some(tokenized_section_id) => {
                // There is already a tokenizer section, so append
                // this tokenizer database to the existing one.
                let tokenizer_section = self.builder.sections.get_mut(tokenized_section_id);
                tokenizer_section.sh_size += section.sh_size;
                tokenizer_section.data = match &tokenizer_section.data {
                    SectionData::Data(data) => {
                        let mut combined_data = data.to_vec();
                        match &section.data {
                            SectionData::Data(new_data) => combined_data.extend(&new_data.to_vec()),
                            _ => unreachable!("Incorrect data type"),
                        };
                        SectionData::Data(Bytes::from(combined_data))
                    }
                    _ => unreachable!("Incorrect data type"),
                };
                section_map.insert(section.id().index(), tokenized_section_id);
            }
            None => {
                // No existing tokenized section in the system image, so use
                // this one.
                add_tokenizer_section = true;
            }
        }

        add_tokenizer_section
    }

    fn is_tokenizer_section(section: &Section) -> bool {
        if section.is_alloc() {
            return false;
        }

        section.name.starts_with(b".pw_tokenizer.")
    }
}

fn get_app_name(path: &Path, index: usize) -> Result<String> {
    let filename = path
        .file_stem()
        .context("Invalid path: No filename found")?
        .to_str()
        .context("Invalid path: Filename is not valid UTF-8")
        .map(|s| s.to_owned())?;

    // Ensure the app name is a valid elf symbol.
    // Replace any invalid characters with `_`.
    // There is no concern over name collisions, as
    // we also add a unique index suffix
    let mut valid_name = String::new();
    let chars = filename.chars();
    for char in chars {
        if char.is_alphanumeric() || char == '_' {
            valid_name.push(char);
        } else {
            valid_name.push('_');
        }
    }
    valid_name.push_str(format!("_{}", index).as_str());

    Ok(valid_name)
}

fn assemble(args: Args) -> Result<()> {
    // println!("Adding kernel image: {}", args.kernel.display());
    let kernel_bytes =
        fs::read(&args.kernel).map_err(|e| anyhow!("Failed to read kernel image: {e}"))?;

    let mut system_image: SystemImage<'_> = SystemImage::new(&*kernel_bytes)?;

    for (index, app) in args.apps.iter().enumerate() {
        // println!("Adding app image: {}", app.display());
        let app_bytes = fs::read(app).map_err(|e| anyhow!("Failed to read app image: {e}"))?;

        let app_name = get_app_name(app, index)?;
        system_image.add_app_image(&*app_bytes, &app_name)?;
    }

    // println!("Writing system image: {}", args.output.display());
    let mut open_options = fs::OpenOptions::new();
    open_options.write(true).create(true).truncate(true);
    let system_file = open_options
        .open(args.output)
        .map_err(|e| anyhow!("Failed to create system image: {e}"))?;
    let mut writer = BufWriter::new(system_file);
    system_image.write(&mut writer)
}

fn main() -> Result<()> {
    let args = Args::parse();
    assemble(args).map_err(|e| anyhow!("{e}"))
}
