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

#[cfg(test)]
mod tests {
    use kernel::{MemoryRegion, MemoryRegionType};
    use unittest::test;

    #[test]
    fn memory_type_is_readable_returns_correct_value() -> unittest::Result<()> {
        unittest::assert_true!(MemoryRegionType::ReadOnlyData.is_readable());
        unittest::assert_true!(MemoryRegionType::ReadWriteData.is_readable());
        unittest::assert_true!(MemoryRegionType::ReadOnlyExecutable.is_readable());
        unittest::assert_true!(MemoryRegionType::ReadWriteExecutable.is_readable());
        unittest::assert_true!(MemoryRegionType::Device.is_readable());
        Ok(())
    }

    #[test]
    fn memory_type_is_writeable_returns_correct_value() -> unittest::Result<()> {
        unittest::assert_false!(MemoryRegionType::ReadOnlyData.is_writeable());
        unittest::assert_true!(MemoryRegionType::ReadWriteData.is_writeable());
        unittest::assert_false!(MemoryRegionType::ReadOnlyExecutable.is_writeable());
        unittest::assert_true!(MemoryRegionType::ReadWriteExecutable.is_writeable());
        unittest::assert_true!(MemoryRegionType::Device.is_writeable());
        Ok(())
    }

    #[test]
    fn memory_type_is_executable_returns_correct_value() -> unittest::Result<()> {
        unittest::assert_false!(MemoryRegionType::ReadOnlyData.is_executable());
        unittest::assert_false!(MemoryRegionType::ReadWriteData.is_executable());
        unittest::assert_true!(MemoryRegionType::ReadOnlyExecutable.is_executable());
        unittest::assert_true!(MemoryRegionType::ReadWriteExecutable.is_executable());
        unittest::assert_false!(MemoryRegionType::Device.is_executable());
        Ok(())
    }

    #[test]
    fn memory_type_has_access_returns_correct_value() -> unittest::Result<()> {
        // These checks are implemented using bit masks/math so the full space
        // is validated explicitly.
        unittest::assert_true!(
            MemoryRegionType::ReadOnlyData.has_access(MemoryRegionType::ReadOnlyData)
        );
        unittest::assert_false!(
            MemoryRegionType::ReadOnlyData.has_access(MemoryRegionType::ReadWriteData)
        );
        unittest::assert_false!(
            MemoryRegionType::ReadOnlyData.has_access(MemoryRegionType::ReadOnlyExecutable)
        );
        unittest::assert_false!(
            MemoryRegionType::ReadOnlyData.has_access(MemoryRegionType::ReadWriteExecutable)
        );
        unittest::assert_false!(
            MemoryRegionType::ReadOnlyData.has_access(MemoryRegionType::Device)
        );

        unittest::assert_true!(
            MemoryRegionType::ReadWriteData.has_access(MemoryRegionType::ReadOnlyData)
        );
        unittest::assert_true!(
            MemoryRegionType::ReadWriteData.has_access(MemoryRegionType::ReadWriteData)
        );
        unittest::assert_false!(
            MemoryRegionType::ReadWriteData.has_access(MemoryRegionType::ReadOnlyExecutable)
        );
        unittest::assert_false!(
            MemoryRegionType::ReadWriteData.has_access(MemoryRegionType::ReadWriteExecutable)
        );
        unittest::assert_false!(
            MemoryRegionType::ReadWriteData.has_access(MemoryRegionType::Device)
        );

        unittest::assert_true!(
            MemoryRegionType::ReadOnlyExecutable.has_access(MemoryRegionType::ReadOnlyData)
        );
        unittest::assert_false!(
            MemoryRegionType::ReadOnlyExecutable.has_access(MemoryRegionType::ReadWriteData)
        );
        unittest::assert_true!(
            MemoryRegionType::ReadOnlyExecutable.has_access(MemoryRegionType::ReadOnlyExecutable)
        );
        unittest::assert_false!(
            MemoryRegionType::ReadOnlyExecutable.has_access(MemoryRegionType::ReadWriteExecutable)
        );
        unittest::assert_false!(
            MemoryRegionType::ReadOnlyExecutable.has_access(MemoryRegionType::Device)
        );

        unittest::assert_true!(
            MemoryRegionType::ReadWriteExecutable.has_access(MemoryRegionType::ReadOnlyData)
        );
        unittest::assert_true!(
            MemoryRegionType::ReadWriteExecutable.has_access(MemoryRegionType::ReadWriteData)
        );
        unittest::assert_true!(
            MemoryRegionType::ReadWriteExecutable.has_access(MemoryRegionType::ReadOnlyExecutable)
        );
        unittest::assert_true!(
            MemoryRegionType::ReadWriteExecutable.has_access(MemoryRegionType::ReadWriteExecutable)
        );
        unittest::assert_false!(
            MemoryRegionType::ReadWriteExecutable.has_access(MemoryRegionType::Device)
        );

        unittest::assert_true!(MemoryRegionType::Device.has_access(MemoryRegionType::ReadOnlyData));
        unittest::assert_true!(
            MemoryRegionType::Device.has_access(MemoryRegionType::ReadWriteData)
        );
        unittest::assert_false!(
            MemoryRegionType::Device.has_access(MemoryRegionType::ReadOnlyExecutable)
        );
        unittest::assert_false!(
            MemoryRegionType::Device.has_access(MemoryRegionType::ReadWriteExecutable)
        );
        unittest::assert_true!(MemoryRegionType::Device.has_access(MemoryRegionType::Device));
        Ok(())
    }

    #[test]
    fn memory_region_allows_access_to_full_region() -> unittest::Result<()> {
        unittest::assert_true!(
            MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1000_0000, 0x2000_0000).has_access(
                &MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1000_0000, 0x2000_0000)
            )
        );

        Ok(())
    }

    #[test]
    fn memory_region_allows_access_to_beginning_region() -> unittest::Result<()> {
        unittest::assert_true!(
            MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1000_0000, 0x2000_0000).has_access(
                &MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1000_0000, 0x1500_0000)
            )
        );

        Ok(())
    }

    #[test]
    fn memory_region_allows_access_to_middle_region() -> unittest::Result<()> {
        unittest::assert_true!(
            MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1000_0000, 0x2000_0000).has_access(
                &MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1200_0000, 0x1500_0000)
            )
        );

        Ok(())
    }

    #[test]
    fn memory_region_allows_access_to_ending_region() -> unittest::Result<()> {
        unittest::assert_true!(
            MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1000_0000, 0x2000_0000).has_access(
                &MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1500_0000, 0x2000_0000)
            )
        );

        Ok(())
    }

    #[test]
    fn memory_region_disallows_access_region_before_start() -> unittest::Result<()> {
        unittest::assert_false!(
            MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1000_0000, 0x2000_0000).has_access(
                &MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x0fff_ffff, 0x2000_0000)
            )
        );

        Ok(())
    }

    #[test]
    fn memory_region_disallows_access_region_after_end() -> unittest::Result<()> {
        unittest::assert_false!(
            MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1000_0000, 0x2000_0000).has_access(
                &MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1000_0000, 0x2000_0001)
            )
        );

        Ok(())
    }

    #[test]
    fn memory_region_disallows_access_to_superset() -> unittest::Result<()> {
        unittest::assert_false!(
            MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1000_0000, 0x2000_0000).has_access(
                &MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x0fff_ffff, 0x2000_0001)
            )
        );

        Ok(())
    }

    #[test]
    fn memory_regions_allows_access_to_subregion_regions() -> unittest::Result<()> {
        const REGIONS: &[MemoryRegion] = &[
            MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1000_0000, 0x2000_0000),
            MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x2000_0000, 0x3000_0000),
        ];

        unittest::assert_true!(MemoryRegion::regions_have_access(
            REGIONS,
            &MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1000_0000, 0x2000_0000)
        ));
        unittest::assert_true!(MemoryRegion::regions_have_access(
            REGIONS,
            &MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x2000_0000, 0x3000_0000)
        ));

        Ok(())
    }

    #[test]
    fn memory_regions_disallows_access_spanning_multiple_regions() -> unittest::Result<()> {
        const REGIONS: &[MemoryRegion] = &[
            MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1000_0000, 0x2000_0000),
            MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x2000_0000, 0x3000_0000),
        ];

        unittest::assert_false!(MemoryRegion::regions_have_access(
            REGIONS,
            &MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1000_0000, 0x3000_0000)
        ));

        Ok(())
    }

    #[test]
    fn memory_regions_allows_access_to_sub_region_type() -> unittest::Result<()> {
        const REGIONS: &[MemoryRegion] = &[
            MemoryRegion::new(
                MemoryRegionType::ReadOnlyExecutable,
                0x1000_0000,
                0x2000_0000,
            ),
            MemoryRegion::new(MemoryRegionType::ReadWriteData, 0x2000_0000, 0x3000_0000),
        ];

        unittest::assert_true!(MemoryRegion::regions_have_access(
            REGIONS,
            &MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x1000_0000, 0x2000_0000)
        ));
        unittest::assert_true!(MemoryRegion::regions_have_access(
            REGIONS,
            &MemoryRegion::new(MemoryRegionType::ReadOnlyData, 0x2000_0000, 0x3000_0000)
        ));

        Ok(())
    }

    #[test]
    fn memory_regions_disallows_access_to_wrong_region_type() -> unittest::Result<()> {
        const REGIONS: &[MemoryRegion] = &[
            MemoryRegion::new(
                MemoryRegionType::ReadOnlyExecutable,
                0x1000_0000,
                0x2000_0000,
            ),
            MemoryRegion::new(MemoryRegionType::ReadWriteData, 0x2000_0000, 0x3000_0000),
        ];

        unittest::assert_false!(MemoryRegion::regions_have_access(
            REGIONS,
            &MemoryRegion::new(MemoryRegionType::ReadWriteData, 0x1000_0000, 0x2000_0000)
        ));
        unittest::assert_false!(MemoryRegion::regions_have_access(
            REGIONS,
            &MemoryRegion::new(
                MemoryRegionType::ReadOnlyExecutable,
                0x2000_0000,
                0x3000_0000
            )
        ));

        Ok(())
    }
}
