import * as React from 'react'

import SvgIcon, { SvgIconProps } from '@material-ui/core/SvgIcon';

export const ExpandedIcon = (props: SvgIconProps) => (
  <SvgIcon {...props} width='24' height='24' viewBox='0 0 24 24' fill='none' xmlns='http://www.w3.org/2000/svg'>
    <g opacity='0.45'>
      <path
        d='M18 10.0001L16.59 8.59009L12 13.1701L7.41 8.59009L6 10.0001L12 16.0001L18 10.0001Z'
        fill='white'
      />
    </g>
  </SvgIcon>
);
